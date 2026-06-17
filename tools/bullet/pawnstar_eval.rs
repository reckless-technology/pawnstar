/*
Reference evaluator for cross-checking the in-engine NNUE inference (src/nnue.cpp) against bullet.

Reads a trained net (bullet's native quantised .bin) and a list of FENs (one per line, on stdin or a
file argument), and prints "<fen> | <eval_cp>" using the SAME king-bucketed ChessBuckets feature formula
bullet trains with and the SAME SCReLU inference. test_nnue compares the engine's eval to this; agreement
within rounding confirms feature indexing, king bucketing, orientation and dequantisation all match.

Usage:  cargo run --release --example pawnstar_eval -- <net.bin> [fens.txt]
*/
use std::io::{BufRead, BufReader, Read};

use bulletformat::ChessBoard;

const HIDDEN: usize = 1024;
const QA: i32 = 255;
const QB: i32 = 64;
const SCALE: i32 = 400;

// King-square -> weight-bank map. MUST be byte-identical to kKingBucketMap in src/nnue.cpp and to
// KING_BUCKETS in pawnstar.rs. All-zero = a single bank (no king buckets); NUM_BUCKETS below must match.
#[rustfmt::skip]
const KING_BUCKETS: [usize; 64] = [
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
];
const NUM_BUCKETS: usize = 1;

fn screlu(x: i16) -> i32 {
    let y = i32::from(x).clamp(0, QA);
    y * y
}

struct Net {
    feature_weights: Vec<i16>, // 768*NUM_BUCKETS * HIDDEN, column-major: feature * HIDDEN + i
    feature_bias: Vec<i16>,    // HIDDEN
    output_weights: Vec<i16>,  // 2 * HIDDEN
    output_bias: i16,
}

impl Net {
    fn load(path: &str) -> Net {
        let mut bytes = Vec::new();
        std::fs::File::open(path).unwrap().read_to_end(&mut bytes).unwrap();
        let vals: Vec<i16> =
            bytes.chunks_exact(2).map(|c| i16::from_le_bytes([c[0], c[1]])).collect();
        let fw = 768 * NUM_BUCKETS * HIDDEN;
        let expected = fw + HIDDEN + 2 * HIDDEN + 1;
        // bullet pads the trailing output_bias tensor to a 64-byte boundary, so the file may be slightly
        // larger than the packed weights; read what we need and ignore any trailing padding.
        assert!(vals.len() >= expected, "net too small: got {} i16, need {}", vals.len(), expected);
        Net {
            feature_weights: vals[0..fw].to_vec(),
            feature_bias: vals[fw..fw + HIDDEN].to_vec(),
            output_weights: vals[fw + HIDDEN..fw + HIDDEN + 2 * HIDDEN].to_vec(),
            output_bias: vals[fw + HIDDEN + 2 * HIDDEN],
        }
    }

    fn eval(&self, board: &ChessBoard) -> i32 {
        let mut us = self.feature_bias.clone();
        let mut them = self.feature_bias.clone();
        // Identical to bullet's ChessBuckets::map_features: each perspective's features are offset by its
        // own king's bucket bank (our_ksq for stm, opp_ksq for ntm; both already perspective-oriented).
        let our_bucket = 768 * KING_BUCKETS[usize::from(board.our_ksq())];
        let opp_bucket = 768 * KING_BUCKETS[usize::from(board.opp_ksq())];
        for (piece, square) in board.into_iter() {
            let c = usize::from(piece & 8 > 0);
            let pc = 64 * usize::from(piece & 7);
            let sq = usize::from(square);
            let stm = our_bucket + [0, 384][c] + pc + sq;
            let ntm = opp_bucket + [384, 0][c] + pc + (sq ^ 56);
            for i in 0..HIDDEN {
                us[i] += self.feature_weights[stm * HIDDEN + i];
                them[i] += self.feature_weights[ntm * HIDDEN + i];
            }
        }
        let mut out: i32 = 0;
        for i in 0..HIDDEN {
            out += screlu(us[i]) * i32::from(self.output_weights[i]);
            out += screlu(them[i]) * i32::from(self.output_weights[HIDDEN + i]);
        }
        out /= QA;
        out += i32::from(self.output_bias);
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
