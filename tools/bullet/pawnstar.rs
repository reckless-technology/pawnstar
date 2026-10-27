/*
Pawnstar NNUE trainer: the shared Reckless architecture

    (768 x NUM_BUCKETS king buckets, horizontally mirrored) -> HIDDEN_SIZE x2 -> OUTPUT_BUCKETS -> 1

SCReLU, matching the in-engine inference in src/nnue.h. One network shape for all the Reckless chess
engines (skill `reckless-nnue`): a net trained here loads in pawnstar, pawnstar-rs, pawnstar-go and
zenith, and an architecture change here is a change owed to all of them.

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
        schedule::{TrainingSchedule, TrainingSteps, lr, wdl},
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

fn main() {
    let data_path = std::env::var("PAWNSTAR_DATA").unwrap_or_else(|_| "data/pawnstar.data".to_string());
    let out_dir = std::env::var("PAWNSTAR_OUT").unwrap_or_else(|_| "checkpoints".to_string());
    let bps: usize = env_or("PAWNSTAR_BPS", 1000);
    let end_sb: usize = env_or("PAWNSTAR_SBS", 30);
    let save_rate: usize = env_or("PAWNSTAR_SAVE_RATE", 10);
    // How much of each target is the game result rather than the eval sigmoid. 0.5 for the shipped nets,
    // whose data is engine self-play where result and eval agree. Human games disagree far more often -
    // a player loses a won position - so a human-game set may want this lower.
    let wdl_weight: f32 = env_or("PAWNSTAR_WDL", 0.5);

    println!(
        "pawnstar trainer: (768x{NUM_BUCKETS}hm -> {HIDDEN_SIZE})x2 -> {OUTPUT_BUCKETS} -> 1, SCReLU, \
         QA={QA} QB={QB} SCALE={SCALE}"
    );

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
        lr_scheduler: lr::StepLR { start: 0.001, gamma: 0.3, step: end_sb / 3 },
        save_rate,
    };

    let settings =
        LocalSettings { threads: 4, test_set: None, output_directory: &out_dir, batch_queue_size: 64 };

    let data_loader = loader::DirectSequentialDataLoader::new(&[&data_path]);

    trainer.run(&schedule, &settings, &data_loader);
}
