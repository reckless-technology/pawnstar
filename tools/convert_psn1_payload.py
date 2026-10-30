#!/usr/bin/env python3
"""Convert a net in the retired 32-byte `PSN1` container into a raw `RNNUE1` payload.

The two containers hold the same trained weights in the same order. Exactly one thing differs in the
payload: PSN1 ended with a single `int16` output bias, and RNNUE1 ends with an `int32` bias per output
bucket (src/nnue.h, skill `reckless-nnue`). Widening one `int16` to `int32` is a sign extension, so the
weights this writes are bit-for-bit the weights that went in and the engine's evaluation is unchanged --
which is what let pawnstar's bench signature stay at 6,601,454 across the format bump in 2026-10-27 (it is
5,617,906 now, with the v20 net).

It writes a *raw* payload, not a net: the RNNUE1 header is then written by `stamp_net`, from that build's
own compiled constants, so no header is ever produced by hand.

    python3 tools/convert_psn1_payload.py old-psn1-net.bin /tmp/raw.bin
    ./build/stamp_net /tmp/raw.bin nnue/converted.bin

No PSN1 net is shipped any more - the last one was pawnstar-v12.bin, retired at release 2.0.0 - so this
is a migration tool for a net held outside the repository.

Only nets with a single output bucket can be converted, which is every PSN1 net there ever was: PSN1 had
no output-bucket field because the architecture had exactly one head.
"""

import argparse
import struct
import sys

PSN1_MAGIC = b"PSN1"
PSN1_HEADER_BYTES = 32
HIDDEN_SIZE = 1024
FEATURES_PER_BUCKET = 768


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("psn1_net", help="a net in the PSN1 container")
    parser.add_argument("raw_payload_out", help="where to write the raw RNNUE1 payload")
    arguments = parser.parse_args()

    with open(arguments.psn1_net, "rb") as net_file:
        image = net_file.read()

    if image[:4] != PSN1_MAGIC:
        sys.exit(f"{arguments.psn1_net}: no 'PSN1' magic -- not a net in the retired container")

    # The PSN1 header's architecture fields, in its own order: magic, version, input size, king buckets,
    # hidden size, qa, qb, scale. Read them rather than assume them, so a mismatched net is caught here.
    (version, features_per_bucket, king_buckets, hidden_size, qa, qb, eval_scale) = struct.unpack_from(
        "<7H", image, 4
    )
    print(
        f"{arguments.psn1_net}: PSN1 v{version}, features_per_bucket {features_per_bucket}, "
        f"king_buckets {king_buckets}, hidden_size {hidden_size}, qa {qa}, qb {qb}, eval_scale {eval_scale}"
    )
    if features_per_bucket != FEATURES_PER_BUCKET or hidden_size != HIDDEN_SIZE:
        sys.exit("only a 768-feature, 1024-wide PSN1 net can be converted")

    feature_weight_bytes = king_buckets * features_per_bucket * hidden_size * 2
    feature_bias_bytes = hidden_size * 2
    output_weight_bytes = 2 * hidden_size * 2
    psn1_payload_bytes = feature_weight_bytes + feature_bias_bytes + output_weight_bytes + 2

    payload = image[PSN1_HEADER_BYTES:]
    if len(payload) < psn1_payload_bytes:
        sys.exit(f"{arguments.psn1_net}: {len(payload)} payload bytes, expected at least {psn1_payload_bytes}")
    # bullet pads its quantised output up to a 64-byte boundary with repeats of the word "bullet"; PSN1
    # kept that tail, RNNUE1 records an exact length and drops it.
    tail = payload[psn1_payload_bytes:]
    if tail and set(tail) - set(b"bullet"):
        sys.exit(f"{arguments.psn1_net}: {len(tail)} trailing bytes that are not bullet's padding")
    payload = payload[:psn1_payload_bytes]

    (output_bias,) = struct.unpack_from("<h", payload, psn1_payload_bytes - 2)
    converted = payload[: psn1_payload_bytes - 2] + struct.pack("<i", output_bias)

    with open(arguments.raw_payload_out, "wb") as raw_file:
        raw_file.write(converted)
    print(
        f"{arguments.raw_payload_out}: {len(converted)} raw RNNUE1 payload bytes "
        f"({len(tail)} bytes of bullet padding dropped, output_bias {output_bias} widened to int32). "
        f"Stamp it with build/stamp_net."
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())
