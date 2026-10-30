/*
Reference evaluator for cross-checking the in-engine NNUE inference (src/nnue.h) against bullet.

Reads a trained net (bullet's native quantised .bin, NOT a stamped RNNUE1 net) and a list of FENs (one per
line, on stdin or a file argument), and prints "<fen> | <eval_cp>" using the SAME horizontally-mirrored
king-bucketed ChessBucketsMirrored feature formula bullet trains with, the SAME MaterialCount output-head
selection and the SAME SCReLU inference. test_nnue compares the engine's eval to this; agreement within
rounding confirms feature indexing, king bucketing, mirroring, orientation, head selection and
dequantisation all match.

Adapted from bullet (https://github.com/jw1912/bullet, at the commit pinned by tools/setup_bullet.sh):
Copyright (c) 2023 Jamie Whiting, MIT licence. The SCReLU inference, the Net field layout and the
dequantisation order follow bullet's `simple` example (examples/simple.rs); the per-piece feature index, the
king-bucket offset and the mirror flip are those of bullet's `Chess768` and `ChessBucketsMirrored` input
types (crates/bullet_lib/src/game/inputs), and the head selection is that of `MaterialCount`
(crates/bullet_lib/src/game/outputs.rs). Its licence text is reproduced in THIRD-PARTY-NOTICES.md at the
root of this repository; this file as a whole is distributed under the repository's GPLv3.

Usage:  cargo run --release --example pawnstar_eval -- <net.bin> [fens.txt]
*/
use std::io::{BufRead, BufReader, Read};

use bulletformat::ChessBoard;

const HIDDEN: usize = 1024;
const QA: i32 = 255;
const QB: i32 = 64;
const SCALE: i32 = 400;

// The mirrored king-bucket map over HALF a board: 32 entries indexed `rank * 4 + mirrored_file`, where
// `mirrored_file = min(file, 7 - file)`. MUST be byte-identical to KING_BUCKETS_HALF in pawnstar.rs and to
// kKingBucketMapHalf in src/nnue.h (test_primitives compares the engine's against the trainer's).
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
const NUM_BUCKETS: usize = 8;

// Material output heads, selected by bullet's `MaterialCount<8>`: `(piece_count - 2) / ceil(32 / 8)`.
const OUTPUT_BUCKETS: usize = 8;

/// bullet's `ChessBucketsMirrored::new` expansion, verbatim:
/// `expanded[sq] = buckets[(sq / 8) * 4 + [0, 1, 2, 3, 3, 2, 1, 0][sq % 8]]`.
fn expand_king_buckets() -> [usize; 64] {
    let mut expanded = [0; 64];
    for (idx, elem) in expanded.iter_mut().enumerate() {
        *elem = KING_BUCKETS_HALF[(idx / 8) * 4 + [0, 1, 2, 3, 3, 2, 1, 0][idx % 8]];
    }
    expanded
}

fn screlu(x: i16) -> i32 {
    let y = i32::from(x).clamp(0, QA);
    y * y
}

struct Net {
    feature_weights: Vec<i16>, // 768*NUM_BUCKETS * HIDDEN, column-major: feature * HIDDEN + i
    feature_bias: Vec<i16>,    // HIDDEN
    output_weights: Vec<i16>,  // OUTPUT_BUCKETS * 2 * HIDDEN, bucket-major, stm half first
    output_bias: Vec<i32>,     // OUTPUT_BUCKETS
    king_buckets: [usize; 64],
}

impl Net {
    fn load(path: &str) -> Net {
        let mut bytes = Vec::new();
        std::fs::File::open(path).unwrap().read_to_end(&mut bytes).unwrap();

        // Byte offsets of bullet's save order for this architecture (tools/bullet/pawnstar.rs's
        // save_format): l0w int16, l0b int16, l1w int16 (transposed to bucket-major), l1b **int32**.
        let fw_bytes = 768 * NUM_BUCKETS * HIDDEN * 2;
        let fb_bytes = HIDDEN * 2;
        let ow_bytes = OUTPUT_BUCKETS * 2 * HIDDEN * 2;
        let ob_bytes = OUTPUT_BUCKETS * 4;
        let needed = fw_bytes + fb_bytes + ow_bytes + ob_bytes;
        // bullet pads the trailing tensor to a 64-byte boundary, so the file may be slightly larger than
        // the packed weights; read what we need and ignore any trailing padding.
        assert!(bytes.len() >= needed, "net too small: got {} bytes, need {}", bytes.len(), needed);

        let read_i16 = |from: usize, count: usize| -> Vec<i16> {
            bytes[from..from + count * 2]
                .chunks_exact(2)
                .map(|c| i16::from_le_bytes([c[0], c[1]]))
                .collect()
        };
        let ow_at = fw_bytes + fb_bytes;
        let ob_at = ow_at + ow_bytes;
        let output_bias = bytes[ob_at..ob_at + ob_bytes]
            .chunks_exact(4)
            .map(|c| i32::from_le_bytes([c[0], c[1], c[2], c[3]]))
            .collect();

        Net {
            feature_weights: read_i16(0, 768 * NUM_BUCKETS * HIDDEN),
            feature_bias: read_i16(fw_bytes, HIDDEN),
            output_weights: read_i16(ow_at, OUTPUT_BUCKETS * 2 * HIDDEN),
            output_bias,
            king_buckets: expand_king_buckets(),
        }
    }

    fn eval(&self, board: &ChessBoard) -> i32 {
        let mut us = self.feature_bias.clone();
        let mut them = self.feature_bias.clone();
        // Identical to bullet's ChessBucketsMirrored::map_features: each perspective takes its own king's
        // bucket bank AND its own mirror flip (7 when that king stands on files e-h), the flip applied to
        // the Chess768 feature index, which is where the square's file bits live.
        let get = |ksq: u8| -> (usize, usize) {
            (if ksq % 8 > 3 { 7 } else { 0 }, 768 * self.king_buckets[usize::from(ksq)])
        };
        let (our_flip, our_bucket) = get(board.our_ksq());
        let (opp_flip, opp_bucket) = get(board.opp_ksq());
        for (piece, square) in board.into_iter() {
            let c = usize::from(piece & 8 > 0);
            let pc = 64 * usize::from(piece & 7);
            let sq = usize::from(square);
            let stm = our_bucket + ((([0, 384][c] + pc + sq)) ^ our_flip);
            let ntm = opp_bucket + ((([384, 0][c] + pc + (sq ^ 56))) ^ opp_flip);
            for i in 0..HIDDEN {
                us[i] += self.feature_weights[stm * HIDDEN + i];
                them[i] += self.feature_weights[ntm * HIDDEN + i];
            }
        }
        // bullet's MaterialCount<OUTPUT_BUCKETS>, and src/nnue.h's OutputBucket.
        let divisor = 32usize.div_ceil(OUTPUT_BUCKETS);
        let bucket = (board.occ().count_ones() as usize - 2) / divisor;
        let weights = &self.output_weights[bucket * 2 * HIDDEN..(bucket + 1) * 2 * HIDDEN];
        let mut out: i32 = 0;
        for i in 0..HIDDEN {
            out += screlu(us[i]) * i32::from(weights[i]);
            out += screlu(them[i]) * i32::from(weights[HIDDEN + i]);
        }
        out /= QA;
        out += self.output_bias[bucket];
        out *= SCALE;
        out /= QA * QB;
        out
    }
}

fn main() {
    let args: Vec<String> = std::env::args().collect();
    let net = Net::load(&args[1]);

    let reader: Box<dyn BufRead> = if args.len() > 2 {
        Box::new(BufReader::new(std::fs::File::open(&args[2]).unwrap()))
    } else {
        Box::new(BufReader::new(std::io::stdin()))
    };

    for line in reader.lines() {
        let fen = line.unwrap();
        let fen = fen.trim();
        if fen.is_empty() {
            continue;
        }
        // ChessBoard::from_str wants "fen | score | wdl"; values are irrelevant for evaluation.
        let record = format!("{fen} | 0 | 0.5");
        match record.parse::<ChessBoard>() {
            Ok(board) => println!("{fen} | {}", net.eval(&board)),
            Err(e) => eprintln!("parse error for '{fen}': {e}"),
        }
    }
}
