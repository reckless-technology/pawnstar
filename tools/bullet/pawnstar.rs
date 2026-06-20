/*
Pawnstar NNUE trainer: arch (768xNUM_BUCKETS king-bucketed -> HIDDEN_SIZE)x2 -> 1, SCReLU, matching the
in-engine inference in src/nnue.cpp (the KING_BUCKETS map below must stay byte-identical to its
kKingBucketMap). Based on bullet's `simple` example. Configured via environment variables:

    PAWNSTAR_DATA   path to a BulletFormat .data file        (default: data/pawnstar.data)
    PAWNSTAR_BPS    batches_per_superbatch (~ dataset/16384)  (default: 1000)
    PAWNSTAR_SBS    end_superbatch                            (default: 30)
    PAWNSTAR_OUT    output checkpoint directory               (default: checkpoints)

The final quantised net is written to <out>/pawnstar-<sbs>/pawnstar-<sbs>.bin in bullet's native
headerless format, which src/nnue.cpp loads directly.
*/
use bullet_lib::{
    game::inputs::{ChessBuckets, get_num_buckets},
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

// King-square -> weight-bank map. MUST be byte-identical to kKingBucketMap in src/nnue.cpp. The shipped net
// is all-zero: a single bank, i.e. no king buckets (plain Chess768); NUM_BUCKETS auto-derives to 1. For a
// bucketed net put the per-square bank indices here (indexed by each perspective's own king square — bullet
// orients our_ksq/opp_ksq per perspective, matching the engine's white/black).
#[rustfmt::skip]
const KING_BUCKETS: [usize; 64] = [
    0, 0, 1, 1, 2, 2, 3, 3,
    0, 0, 1, 1, 2, 2, 3, 3,
    0, 0, 1, 1, 2, 2, 3, 3,
    0, 0, 1, 1, 2, 2, 3, 3,
    0, 0, 1, 1, 2, 2, 3, 3,
    0, 0, 1, 1, 2, 2, 3, 3,
    0, 0, 1, 1, 2, 2, 3, 3,
    0, 0, 1, 1, 2, 2, 3, 3,
];
const NUM_BUCKETS: usize = get_num_buckets(&KING_BUCKETS);

fn env_or<T: std::str::FromStr>(key: &str, default: T) -> T {
    std::env::var(key).ok().and_then(|v| v.parse().ok()).unwrap_or(default)
}

fn main() {
    let data_path = std::env::var("PAWNSTAR_DATA").unwrap_or_else(|_| "data/pawnstar.data".to_string());
    let out_dir = std::env::var("PAWNSTAR_OUT").unwrap_or_else(|_| "checkpoints".to_string());
    let bps: usize = env_or("PAWNSTAR_BPS", 1000);
    let end_sb: usize = env_or("PAWNSTAR_SBS", 30);
    let save_rate: usize = env_or("PAWNSTAR_SAVE_RATE", 10);

    let mut trainer = ValueTrainerBuilder::default()
        .dual_perspective()
        .optimiser(AdamW)
        .inputs(ChessBuckets::new(KING_BUCKETS))
        // Save order/quantisation must match src/nnue.h (Network layout). l1w is NOT transposed (no output
        // buckets). l0w grows to 768*NUM_BUCKETS feature rows, laid out bucket-major (bucket*768 + feat).
        .save_format(&[
            SavedFormat::id("l0w").round().quantise::<i16>(QA),
            SavedFormat::id("l0b").round().quantise::<i16>(QA),
            SavedFormat::id("l1w").round().quantise::<i16>(QB),
            SavedFormat::id("l1b").round().quantise::<i16>(QA * QB),
        ])
        .loss_fn(|output, target| output.sigmoid().squared_error(target))
        .build(|builder, stm_inputs, ntm_inputs| {
            let l0 = builder.new_affine("l0", 768 * NUM_BUCKETS, HIDDEN_SIZE);
            let l1 = builder.new_affine("l1", 2 * HIDDEN_SIZE, 1);
            // SCReLU activation (matches src/nnue.cpp).
            let stm_hidden = l0.forward(stm_inputs).screlu();
            let ntm_hidden = l0.forward(ntm_inputs).screlu();
            let hidden_layer = stm_hidden.concat(ntm_hidden);
            l1.forward(hidden_layer)
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
        wdl_scheduler: wdl::ConstantWDL { value: 0.5 },
        lr_scheduler: lr::StepLR { start: 0.001, gamma: 0.3, step: end_sb / 3 },
        save_rate,
    };

    let settings =
        LocalSettings { threads: 4, test_set: None, output_directory: &out_dir, batch_queue_size: 64 };

    let data_loader = loader::DirectSequentialDataLoader::new(&[&data_path]);

    trainer.run(&schedule, &settings, &data_loader);
}
