# AWS GPU training for Pawnstar NNUE

Reviewable scripts to run NNUE training on an EC2 GPU instance with **your own** AWS credentials. They run
*locally* (you never paste secrets into a sandbox), and the instance receives your code via `scp` — so it
needs **no GitHub token and no AWS keys**.

## Why a GPU box

bullet's CUDA backend needs a driver supporting **CUDA ≥ 12.3 (driver ≥ 545)**. The **Deep Learning Base
OSS Nvidia Driver AMI (Ubuntu 22.04)** ships that driver + CUDA toolkit preinstalled, so there's nothing to
build but Rust/bullet. CPU training works too (`BULLET_FEATURES=""`) but is far slower — fine only for a
tiny-data screen. Disk: the full v8 shuffled file is **~74 GB**, so the default root volume is **200 GB**.

## One-time prerequisites (on your machine)

- AWS CLI v2, configured (`aws configure`) with an IAM identity allowed to manage EC2
  (`RunInstances`, `Describe*`, `CreateKeyPair`, `*SecurityGroup*`, `TerminateInstances`).
- `git`, `ssh`, `scp`, `curl`.

## Cost

`g5.xlarge` (1× A10G) ≈ **\$1/hr** on-demand, ~\$0.40/hr spot (`SPOT=1`). A screen run is a couple of hours;
a full 2.31B-position train is the better part of a day. **~\$5–15 typical.** Always `teardown.sh` when done.

## Quick start

```bash
# 1) Provision + upload code (prints a plan, asks to confirm). Leaves the box ready, runs nothing heavy.
tools/aws/launch_training.sh                         # defaults: us-east-1, g5.xlarge, 200 GB

# It prints the ssh command and the staged bootstrap commands. SSH in, then:
RUN=setup bash ~/pawnstar/tools/aws/bootstrap_train.sh                       # CUDA check, Rust, bullet
RUN=data  SHARDS="11128 12128 12932 13227" bash ~/pawnstar/tools/aws/bootstrap_train.sh   # download+shuffle
RUN=train SBS=120 bash ~/pawnstar/tools/aws/bootstrap_train.sh              # -> ~/pawnstar_nnue/out.bin

# 2) Pull the net back and SPRT it locally (see ../run_sprt.sh):
scp -i tools/aws/<key>.pem ubuntu@<ip>:~/pawnstar_nnue/out.bin ./

# 3) ALWAYS tear down to stop billing:
REGION=us-east-1 IID=i-... KEY_NAME=<key> SG_ID=sg-... bash tools/aws/teardown.sh
#   or sweep everything tagged:   REGION=us-east-1 ALL=1 bash tools/aws/teardown.sh
```

To run the whole thing unattended, pass `RUN=setup,data,train` to `launch_training.sh` (and `SHARDS`/`SBS`).

## Data scope

`SHARDS` lists PlentyChess shard ids from
<https://huggingface.co/datasets/Yoshie2000/plentychess_data_bulletformat>. Default is **one shard** (~370M
positions) for a fast screen. The shipped **v8** pool is v6's 818M + shards `11128 12128 12932 13227`
(~2.31B total) trained at `SBS=120`; see [../../nnue/README.md](../../nnue/README.md) §7 for the exact recipe
and the v6 base pool. Some shards are corrupt — `bootstrap_train.sh` validates and skips them.

## int8 feature weights — closed (don't retry)

The int8 *feature-weight* experiment is finished and **rejected** (−8 Elo SPRT with a lossless
quantisation-aware net); the engine support and trainer clipping were removed. Don't reintroduce it. See
[../../nnue/int8_quant_study.md](../../nnue/int8_quant_study.md). (The int8 *output* layer is shipped and
unaffected.)

## Troubleshooting

- **`RunInstances ... Unsupported`** — the GPU type isn't offered in that region/AZ (e.g. `g5` in
  `us-west-1`). The launcher now pre-checks availability and picks a valid AZ/subnet, failing early with
  alternatives. Use a GPU-rich region or a widely-available type:
  ```bash
  REGION=us-west-2 tools/aws/launch_training.sh        # or us-east-1
  INSTANCE_TYPE=g4dn.xlarge tools/aws/launch_training.sh   # T4, available almost everywhere, cheaper
  ```
- **`VcpuLimitExceeded` / 0 vCPU quota for G instances** — new accounts often have a 0 quota for
  "Running On-Demand G and VT instances". Request an increase in Service Quotas (≥ 4 vCPUs) for that region.
- **Cleanup after a failed launch** (leftover key pair + security group are free but tidy up anyway):
  ```bash
  REGION=<region> ALL=1 tools/aws/teardown.sh          # sweeps tagged SGs + pawnstar-train-* key pairs
  ```

## Security notes

- Credentials stay on your machine; the instance gets only your code (scp) and public data.
- SSH ingress is locked to your current public IP (`/32`) at launch. If your IP changes, re-authorize.
- The generated `.pem` is written under `tools/aws/` (gitignored) — keep it private; `teardown.sh` deletes
  the key pair in AWS but not the local file.
