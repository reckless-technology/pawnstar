/*
Pawnstar NNUE trainer: the shared Reckless architecture

    (768 x NUM_BUCKETS king buckets, horizontally mirrored) -> HIDDEN_SIZE x2 -> OUTPUT_BUCKETS -> 1

SCReLU, matching the in-engine inference in src/nnue.h. One network shape for all the Reckless chess
engines (skill `reckless-nnue`): a net trained here loads in pawnstar, pawnstar-rs, pawnstar-go and
zenith, and an architecture change here is a change owed to all of them.

Adapted from bullet's `simple` example (examples/simple.rs, https://github.com/jw1912/bullet, at the commit
pinned by tools/setup_bullet.sh): Copyright (c) 2023 Jamie Whiting, MIT licence. The imports, the QA/QB/SCALE
constants, the save-format list, the loss function, the two-layer SCReLU network and the schedule and
loader layout follow that example. Its licence text is reproduced in THIRD-PARTY-NOTICES.md at the root of
this repository; this file as a whole is distributed under the repository's GPLv3.

Two things differ from the shape that produced the shipped v12 net, each measured in zenith on this
hardware and SPRT-gated:

  * horizontal king mirroring          +14.3 Elo at fixed depth
  * 8 material output heads            +14.7 Elo at fixed depth

Both only paid past ~1.4B training positions - at 190M they were neutral or negative, which is why v12
has neither. Mirroring is also worth more than its Elo suggests: halving the king-placement space gives
twice the effective data per bucket at the same file size.

The pinned bullet supports both natively, no fork and no patch: `ChessBucketsMirrored::new` takes the
32-entry half-board map and expands it itself, and `outputs::MaterialCount::<N>` buckets by
`(piece_count - 2) / ceil(32 / N)`, which is what src/nnue.h's `OutputBucket` computes.

**KING_BUCKETS_HALF below is the one place the bucketing is written down for both sides.** src/nnue.h
holds the same 32 numbers as `kKingBucketMapHalf` and *generates* its 64-entry map from them with
bullet's own expansion arithmetic, and `test_primitives` reads this array out of this file and compares
it with that one. So the trainer and the engine cannot drift: a map that drifts trains one network and
runs another, and the net header's `king_bucket_map_crc` catches that only after the training run has
already been spent.

Configured via environment variables:

    PAWNSTAR_DATA       path to a BulletFormat .data file         (default: data/pawnstar.data)
    PAWNSTAR_BPS        batches_per_superbatch                    (default: 1000)
    PAWNSTAR_SBS        end_superbatch                            (default: 30)
    PAWNSTAR_LR_STEP    superbatches per learning-rate drop       (default: PAWNSTAR_SBS / 3)
    PAWNSTAR_OUT        output checkpoint directory               (default: checkpoints)
    PAWNSTAR_SAVE_RATE  checkpoint every this many superbatches    (default: 10)
    PAWNSTAR_WDL        how much of the target is the game result  (default: 0.5)

A batch is 16,384 positions. The convention these nets are trained on is a superbatch of about 60M of
them - PAWNSTAR_BPS near 3662 - which leaves PAWNSTAR_SBS to set how many passes are made over a pool
larger than that. It is NOT one superbatch per pass over the data: this line used to read
`~ dataset/16384`, which is exactly that, and so makes PAWNSTAR_SBS the epoch count directly. The two
coincide at a 60M pool, which is how the wrong one survived; past that they diverge by pool / 60M.

PAWNSTAR_WDL weighs the game result against the eval sigmoid in each training target. 0.5 suits engine
self-play, where the result and the eval agree; human games disagree far more often - a player loses a
won position - so a human-game pool wants it lower.

The learning rate is a step schedule: it starts at 0.001 and is multiplied by 0.3 every
PAWNSTAR_LR_STEP superbatches, three times over a default-shaped run. bullet's StepLR computes
`start * gamma^((superbatch - 1) / step)`, so drop N first applies at superbatch `N * step + 1` -
and with the default `step = PAWNSTAR_SBS / 3` the third drop lands at `PAWNSTAR_SBS + 1` whenever
PAWNSTAR_SBS divides by 3, one superbatch past the end of the run. Every net this project trained
before 2026-10-29 finished with 0, 1 or 2 superbatches at its lowest rate, and the 819-superbatch
run got none at all: resuming that one for 55 superbatches with the step pinned at 273 moved
held-out loss 0.012300 -> 0.012193, **0.88%**, two thirds of it in the first superbatch after the
drop and all of it within about 30. A drop's tail does not need to be long, but it has to exist.

So this trainer PRINTS the schedule it will follow and REFUSES to start one whose last drop cannot
apply, naming the arithmetic and the fix; it also prints the rate it applies at every superbatch,
which bullet itself does not (it prints the scheduler's description instead, which is the
configuration and not the behaviour). PAWNSTAR_LR_STEP pins the step independently of the run
length, which is both how a truncated schedule is fixed and how a resume holds the lowest rate.
PAWNSTAR_LR_ALLOW_UNREACHABLE_DROP=1 downgrades the refusal to a warning, for reproducing a
historical run on its original schedule and for nothing else.

The default step is deliberately still PAWNSTAR_SBS / 3. Changing it would move all three drops and
so make every past run's schedule unreproducible from this file, and one measurement says only that
the tail must exist and that ~30 superbatches realises it - not what the best spacing is.

The final quantised net is written to <out>/pawnstar-<sbs>/pawnstar-<sbs>.bin in bullet's native
headerless format. **Stamp it before the engine will load it**: `build/stamp_net <raw> <out.bin>` wraps
it in the RNNUE1 container, writing the header from the stamping build's own compiled constants and
dropping the padding bullet appends. See nnue/README.md section 2.
*/
use bullet_lib::{
    game::{inputs::ChessBucketsMirrored, inputs::get_num_buckets, outputs::MaterialCount},
    nn::optimiser::AdamW,
    trainer::{
        save::SavedFormat,
        schedule::{TrainingSchedule, TrainingSteps, lr, lr::LrScheduler, wdl},
        settings::LocalSettings,
    },
    value::{ValueTrainerBuilder, loader},
};

const HIDDEN_SIZE: usize = 1024;
const SCALE: i32 = 400;
const QA: i16 = 255;
const QB: i16 = 64;

/// Material output heads. bullet's `MaterialCount<N>` buckets by `(piece_count - 2) / ceil(32 / N)`, so
/// at 8 the divisor is 4 and the bucket is `(piece_count - 2) / 4` - exactly `OutputBucket` in
/// src/nnue.h. MUST equal that build's `kNumOutputBuckets`, which the net header stamps and the engine's
/// loader checks.
const OUTPUT_BUCKETS: usize = 8;

/// The learning-rate schedule: start at 0.001 and multiply by 0.3 at each drop. Named constants rather
/// than literals at the TrainingSchedule, because check_lr_schedule below prints the rate each drop
/// reaches and must not be able to print numbers the run does not apply.
const LR_START: f32 = 0.001;
const LR_GAMMA: f32 = 0.3;

/// Superbatches at the lowest rate that measurement showed is enough to realise a drop.
///
/// The 819-superbatch run was resumed with its step pinned at 273, so superbatch 820 was its first at
/// `0.001 * 0.3^3`. Held-out loss on a fixed 100,141,613-position validation set: 0.012300 at 819,
/// 0.012229 at 820 (**66%** of the whole step in one superbatch), 0.012202 at 830, 0.012197 at 840,
/// 0.012193 at 850, and flat at 860 and 870. The second drop had the same shape: -2.31% realised within
/// ~40 superbatches, then 239 superbatches that moved nothing. So a schedule that reaches its last drop
/// with fewer than this many superbatches left has paid for a stage it will not collect.
const USEFUL_TAIL_SUPERBATCHES: usize = 30;

// King-square -> weight-bank map, over HALF a board: 32 entries indexed `rank * 4 + mirrored_file`,
// where `mirrored_file = min(file, 7 - file)`. The bank is `half * 4 + mirrored_file` with
// `half = rank / 4` - four mirrored files by two board halves, eight banks.
//
// MUST stay byte-identical to kKingBucketMapHalf in src/nnue.h, which test_primitives checks by reading
// this array out of this very file. bullet expands these 32 to the 64 it indexes as
// `expanded[sq] = KING_BUCKETS_HALF[(sq / 8) * 4 + [0,1,2,3,3,2,1,0][sq % 8]]`, and src/nnue.h's
// ExpandMirroredKingBucketMap does the same arithmetic, so the two 64-entry maps are the same map.
//
// Note this is per-FILE, where the v12 map was per file-PAIR (`0,0,1,1,2,2,3,3`): mirroring has already
// folded files e-h onto d-a, so there are only four distinct files left to bucket. That makes the two
// maps different architectures rather than one map with a switch, which is why turning mirroring on bumps
// the net format version.
#[rustfmt::skip]
const KING_BUCKETS_HALF: [usize; 32] = [
    0, 1, 2, 3,
    0, 1, 2, 3,
    0, 1, 2, 3,
    0, 1, 2, 3,
    4, 5, 6, 7,
    4, 5, 6, 7,
    4, 5, 6, 7,
    4, 5, 6, 7,
];
const NUM_BUCKETS: usize = get_num_buckets(&KING_BUCKETS_HALF);

fn env_or<T: std::str::FromStr>(key: &str, default: T) -> T {
    std::env::var(key).ok().and_then(|v| v.parse().ok()).unwrap_or(default)
}

/// What a `StepLR` of `step` superbatches actually does over a run of `end_sb` superbatches.
///
/// `StepLR::lr` is `start * gamma.powi((superbatch - 1) / step)`, so drop N first applies at superbatch
/// `N * step + 1`. The run therefore REACHES `(end_sb - 1) / step` drops while the schedule's shape
/// INTENDS `end_sb / step` of them, and those two differ by one exactly when `step` divides `end_sb` -
/// which `step = end_sb / 3` produces for every `end_sb` divisible by 3. The drop that goes missing is
/// the last one, which the measurement behind USEFUL_TAIL_SUPERBATCHES says is the most valuable.
struct LrScheduleShape {
    /// `end_sb / step`: the drops the schedule's shape asks for.
    intended_drops: usize,
    /// `(end_sb - 1) / step`: the drops the run gets to.
    reached_drops: usize,
    /// `intended_drops * step + 1`: where the intended last drop would first apply.
    intended_last_drop_superbatch: usize,
    /// Superbatches spent at the lowest rate the run actually reaches.
    tail_superbatches: usize,
}

fn lr_schedule_shape(end_sb: usize, step: usize) -> LrScheduleShape {
    let intended_drops = end_sb / step;
    let reached_drops = end_sb.saturating_sub(1) / step;
    LrScheduleShape {
        intended_drops,
        reached_drops,
        intended_last_drop_superbatch: intended_drops * step + 1,
        tail_superbatches: end_sb - reached_drops * step,
    }
}

/// The rate `StepLR` applies once `drops` drops have been taken - computed exactly as StepLR does, in
/// f32, so a printed rate is the rate.
fn lr_after_drops(drops: usize) -> f32 {
    LR_START * LR_GAMMA.powi(drops as i32)
}

/// Prints the learning-rate schedule the run will actually follow, and **exits non-zero** when its last
/// drop cannot apply.
///
/// This exists because that failure is otherwise silent: bullet prints the scheduler's description
/// ("start 0.001 gamma 0.3 drop every 273 superbatches"), the run completes, the net is written, and
/// nothing anywhere says that the final stage of the schedule never ran. Measured, that stage is worth
/// about 0.88% of held-out loss - roughly eight times what a 59%-larger training pool bought - so the
/// house rule applies and this fails closed, as tools/pawnstar-match.sh refuses a match rather than
/// assume a machine is idle when it cannot tell.
fn check_lr_schedule(end_sb: usize, step: usize) {
    if step == 0 {
        eprintln!("LR SCHEDULE REFUSED: the step is 0, which StepLR divides by.");
        eprintln!("  end_superbatch (PAWNSTAR_SBS) = {end_sb}, so the default PAWNSTAR_SBS / 3 is 0.");
        eprintln!("  Set PAWNSTAR_LR_STEP to a positive number, or raise PAWNSTAR_SBS to 3 or more.");
        std::process::exit(1);
    }

    let shape = lr_schedule_shape(end_sb, step);
    let intended = shape.intended_drops;
    let reached = shape.reached_drops;
    let last_sb = shape.intended_last_drop_superbatch;
    let tail = shape.tail_superbatches;
    let reached_rate = lr_after_drops(reached);
    let intended_rate = lr_after_drops(intended);
    let derived =
        if std::env::var("PAWNSTAR_LR_STEP").is_ok() { "PAWNSTAR_LR_STEP" } else { "PAWNSTAR_SBS / 3" };

    println!("LR SCHEDULE: start {LR_START} gamma {LR_GAMMA} step {step} ({derived}) end_sb {end_sb}");
    println!("  StepLR is start * gamma^((superbatch - 1) / step): drop N applies from N * step + 1:");
    for drop in 1..=intended {
        let at = drop * step + 1;
        let rate = lr_after_drops(drop);
        if at > end_sb {
            println!("    drop {drop} at superbatch {at} -> lr {rate:.8}  NEVER APPLIES (run ends {end_sb})");
        } else {
            println!("    drop {drop} at superbatch {at} -> lr {rate:.8}");
        }
    }

    if reached < intended {
        // Two fixes, offered because they trade differently. The largest step that still fires every
        // intended drop keeps the run length but leaves a short tail; the longer run keeps the step,
        // and so the shape any earlier run was compared against, and gets a tail worth collecting.
        // The suggestion's own shape is recomputed and printed rather than assumed, because lowering
        // the step can fit an extra drop in and the message must not claim otherwise.
        let suggested_step = (end_sb - 1) / intended;
        let suggested = lr_schedule_shape(end_sb, suggested_step);
        let suggested_drops: Vec<String> =
            (1..=suggested.intended_drops).map(|d| (d * suggested_step + 1).to_string()).collect();
        let suggested_drops = suggested_drops.join("/");
        let suggested_tail = suggested.tail_superbatches;
        let suggested_rate = lr_after_drops(suggested.reached_drops);
        let longer_run = last_sb + USEFUL_TAIL_SUPERBATCHES - 1;

        eprintln!("LR SCHEDULE REFUSED: the last learning-rate drop never applies.");
        eprintln!("  end_superbatch (PAWNSTAR_SBS) = {end_sb},  step ({derived}) = {step}");
        eprintln!("  drops intended: {end_sb} / {step} = {intended}");
        eprintln!("  drops reached:  ({end_sb} - 1) / {step} = {reached}");
        eprintln!("  drop {intended} would first apply at superbatch {last_sb}, past the run's end.");
        eprintln!("  The run would finish at lr {reached_rate:.8}, not {intended_rate:.8}, having");
        eprintln!("  paid for a stage it never reaches. Measured once, on the 819-superbatch run's");
        eprintln!("  third drop, that stage was worth 0.88% of held-out loss.");
        eprintln!("  Fix it:");
        // Offered only when it passes this same guard: on a short run, lowering the step enough to
        // fit the last drop can fit one more besides, and a suggestion that would itself be refused
        // is worse than none.
        if suggested.reached_drops == suggested.intended_drops {
            eprintln!("    PAWNSTAR_LR_STEP={suggested_step}: drops at {suggested_drops}, then");
            eprintln!("      {suggested_tail} superbatch(es) at the lowest rate {suggested_rate:.8}");
        }
        eprintln!("    PAWNSTAR_SBS={longer_run}: step {step} unchanged, drop {intended} at {last_sb},");
        eprintln!("      then {USEFUL_TAIL_SUPERBATCHES} superbatches at {intended_rate:.8}");
        eprintln!("  To reproduce a historical run on its truncated schedule, and for nothing else,");
        eprintln!("  set PAWNSTAR_LR_ALLOW_UNREACHABLE_DROP=1; that makes this a warning instead.");
        if std::env::var("PAWNSTAR_LR_ALLOW_UNREACHABLE_DROP").is_err() {
            std::process::exit(1);
        }
        eprintln!("LR SCHEDULE WARNING: continuing: PAWNSTAR_LR_ALLOW_UNREACHABLE_DROP is set.");
    }

    // The tail is reported on every run, refused or not: a schedule whose drops all fire can still end
    // one superbatch after the last of them, which is what the 193- and 772-superbatch runs did.
    if reached == 0 {
        let start_rate = lr_after_drops(0);
        println!("  no drop applies: all {end_sb} superbatches run at lr {start_rate:.8}.");
        return;
    }
    println!("  {reached}/{intended} drops apply; {tail} superbatch(es) at lr {reached_rate:.8}.");
    if tail < USEFUL_TAIL_SUPERBATCHES {
        println!("  WARNING: that tail is shorter than the ~{USEFUL_TAIL_SUPERBATCHES} superbatches a drop");
        println!("  needs to be realised, so most of the last drop will not be collected. Lower");
        println!("  PAWNSTAR_LR_STEP or raise PAWNSTAR_SBS to spend the stage you have paid for.");
    }
}

/// Wraps the real scheduler and prints the learning rate at the first batch of every superbatch.
///
/// bullet prints the scheduler's *description* in its preamble but never the rate it applies, so the
/// log of a finished run records the configuration and not the behaviour - and on a resume there is
/// nothing in it showing whether the scheduler was fed the ABSOLUTE superbatch number or one relative
/// to `start_superbatch`. At `step` 273 a resume at 541 must read 0.0003 (one drop already taken) if
/// the index is absolute and 0.001 if it is relative, and a relative index would move the second drop
/// from 547 to 814, training the net on a schedule nobody intended. This makes that observable rather
/// than inferred, records the whole rate series for the run, and is what confirmed the fixed schedule
/// from the outside ("LR CHECK: superbatch 820 -> lr 0.00002700" on all 55 superbatches of the resume).
/// check_lr_schedule above is static arithmetic over the configuration; this is the rate itself.
#[derive(Clone, Debug)]
struct LoggedLR<T: LrScheduler>(T);

impl<T: LrScheduler> LrScheduler for LoggedLR<T> {
    fn lr(&self, batch: usize, superbatch: usize) -> f32 {
        let rate = self.0.lr(batch, superbatch);
        if batch == 0 {
            println!("LR CHECK: superbatch {superbatch} -> lr {rate:.8}");
        }
        rate
    }

    fn colourful(&self) -> String {
        self.0.colourful()
    }
}

fn main() {
    let data_path = std::env::var("PAWNSTAR_DATA").unwrap_or_else(|_| "data/pawnstar.data".to_string());
    let out_dir = std::env::var("PAWNSTAR_OUT").unwrap_or_else(|_| "checkpoints".to_string());
    let bps: usize = env_or("PAWNSTAR_BPS", 1000);
    let end_sb: usize = env_or("PAWNSTAR_SBS", 30);
    let save_rate: usize = env_or("PAWNSTAR_SAVE_RATE", 10);
    // Superbatches per learning-rate drop. Defaulted from the run length, and pinnable because the
    // two are not the same decision: raising PAWNSTAR_SBS to continue a run MOVES the derived step's
    // drops rather than extending the run past them, so continuing an 819-superbatch run (step 273,
    // drops at 274/547/820) to 900 derives step 300 and puts superbatch 820 back on the SECOND-drop
    // rate it had just spent 273 superbatches at. Pinning 273 keeps `819 / 273 = 3` and `899 / 273 = 3`.
    let lr_step: usize = env_or("PAWNSTAR_LR_STEP", end_sb / 3);
    // How much of each target is the game result rather than the eval sigmoid. 0.5 for the shipped nets,
    // whose data is engine self-play where result and eval agree. Human games disagree far more often -
    // a player loses a won position - so a human-game set may want this lower.
    let wdl_weight: f32 = env_or("PAWNSTAR_WDL", 0.5);

    println!(
        "pawnstar trainer: (768x{NUM_BUCKETS}hm -> {HIDDEN_SIZE})x2 -> {OUTPUT_BUCKETS} -> 1, SCReLU, \
         QA={QA} QB={QB} SCALE={SCALE}"
    );

    // Before the GPU, the data file or the optimiser: a schedule whose last drop cannot apply is a
    // run whose final stage is silently missing, and the run costs hours.
    check_lr_schedule(end_sb, lr_step);

    let mut trainer = ValueTrainerBuilder::default()
        .dual_perspective()
        .optimiser(AdamW)
        .inputs(ChessBucketsMirrored::new(KING_BUCKETS_HALF))
        .output_buckets(MaterialCount::<OUTPUT_BUCKETS>)
        // Save order and quantisation must match the RNNUE1 payload src/nnue.h reads:
        //   l0w  int16[768 * NUM_BUCKETS][HIDDEN_SIZE]  bucket-major rows, QA
        //   l0b  int16[HIDDEN_SIZE]                                        QA
        //   l1w  int16[OUTPUT_BUCKETS][2 * HIDDEN_SIZE]  stm half first,   QB
        //   l1b  int32[OUTPUT_BUCKETS]                                     QA*QB
        //
        // `.transpose()` on l1w is what makes each output head a contiguous 2*HIDDEN_SIZE run: bullet
        // holds l1's weights column-major as (OUTPUT_BUCKETS rows x 2*HIDDEN_SIZE cols), and the engine
        // wants bucket-major so selecting a head is a pointer offset. It is a no-op at one bucket, which
        // is why the v12 trainer could leave it out.
        //
        // l1b is quantised to **i32**, not i16. The trained values fit i16 comfortably, but the width is
        // part of the shared container - every engine reads int32[output_buckets] - so emitting i16 here
        // would produce a payload two bytes short per head and a net every engine refuses.
        .save_format(&[
            SavedFormat::id("l0w").round().quantise::<i16>(QA),
            SavedFormat::id("l0b").round().quantise::<i16>(QA),
            SavedFormat::id("l1w").round().quantise::<i16>(QB).transpose(),
            SavedFormat::id("l1b").round().quantise::<i32>(i32::from(QA) * i32::from(QB)),
        ])
        .loss_fn(|output, target| output.sigmoid().squared_error(target))
        .build(|builder, stm_inputs, ntm_inputs, output_buckets| {
            let l0 = builder.new_affine("l0", 768 * NUM_BUCKETS, HIDDEN_SIZE);
            let l1 = builder.new_affine("l1", 2 * HIDDEN_SIZE, OUTPUT_BUCKETS);
            // SCReLU activation (matches src/nnue.h).
            let stm_hidden = l0.forward(stm_inputs).screlu();
            let ntm_hidden = l0.forward(ntm_inputs).screlu();
            // stm first, then ntm: the engine's output layer weights the side-to-move accumulator with
            // the first HIDDEN_SIZE weights of the selected head.
            let hidden_layer = stm_hidden.concat(ntm_hidden);
            l1.forward(hidden_layer).select(output_buckets)
        });

    let schedule = TrainingSchedule {
        net_id: "pawnstar".to_string(),
        eval_scale: SCALE as f32,
        steps: TrainingSteps {
            batch_size: 16_384,
            batches_per_superbatch: bps,
            start_superbatch: 1,
            end_superbatch: end_sb,
        },
        wdl_scheduler: wdl::ConstantWDL { value: wdl_weight },
        lr_scheduler: LoggedLR(lr::StepLR { start: LR_START, gamma: LR_GAMMA, step: lr_step }),
        save_rate,
    };

    let settings =
        LocalSettings { threads: 4, test_set: None, output_directory: &out_dir, batch_queue_size: 64 };

    let data_loader = loader::DirectSequentialDataLoader::new(&[&data_path]);

    trainer.run(&schedule, &settings, &data_loader);
}
