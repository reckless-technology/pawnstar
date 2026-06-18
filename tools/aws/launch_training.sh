#!/usr/bin/env bash
# Launch a GPU EC2 instance for Pawnstar NNUE training and push the local repo to it.
#
# Runs LOCALLY on your machine with your own AWS CLI credentials (this script never sees a secret it
# wasn't given). It provisions an instance, then copies your working tree to it via scp — so the instance
# needs NO GitHub token and NO AWS keys. After it finishes you SSH in and run the staged training, or pass
# RUN=setup,data,train to have the bootstrap run them now.
#
# Safe by default: prints a plan and waits for confirmation (skip with CONFIRM=1); tags everything
# Project=pawnstar-nnue so tools/aws/teardown.sh can clean up; restricts SSH to your current public IP.
#
# Usage:   tools/aws/launch_training.sh
# Common env (all optional, with defaults):
#   REGION           AWS region                         (default: aws configure get region, else us-east-1)
#   INSTANCE_TYPE    GPU instance                        (default: g5.xlarge — 1x A10G 24GB, ~\$1/hr)
#   VOLUME_GB        root EBS size in GB                 (default: 200 — shuffled data alone is ~74 GB)
#   SPOT             1 = request a spot instance (~-60%) (default: 0)
#   KEY_NAME         existing EC2 key pair to reuse      (default: create pawnstar-train-<rand>, save .pem)
#   GIT_REF          repo ref to ship to the instance    (default: HEAD)
#   RUN              comma list of bootstrap stages to run now: setup,data,train (default: empty = none;
#                    instance is left provisioned + code uploaded so you can review before the long run)
#   CONFIRM          1 = skip the confirmation prompt
#   AMI_ID           override the auto-resolved Deep Learning Base AMI
set -euo pipefail

REGION="${REGION:-$(aws configure get region 2>/dev/null || echo us-east-1)}"
INSTANCE_TYPE="${INSTANCE_TYPE:-g5.xlarge}"
VOLUME_GB="${VOLUME_GB:-200}"
SPOT="${SPOT:-0}"
GIT_REF="${GIT_REF:-HEAD}"
RUN="${RUN:-}"
TAG="Project=pawnstar-nnue"
REPO="$(cd "$(dirname "$0")/../.." && pwd)"
SSH_USER="ubuntu"

command -v aws >/dev/null || { echo "error: aws CLI not found on PATH"; exit 1; }
aws sts get-caller-identity >/dev/null || { echo "error: AWS credentials not configured"; exit 1; }

echo "[1/7] resolving Deep Learning Base GPU AMI in $REGION"
AMI_ID="${AMI_ID:-$(aws ec2 describe-images --region "$REGION" --owners amazon \
    --filters "Name=name,Values=Deep Learning Base OSS Nvidia Driver GPU AMI (Ubuntu 22.04)*" \
              "Name=state,Values=available" \
    --query 'sort_by(Images,&CreationDate)[-1].ImageId' --output text)}"
[ "$AMI_ID" != "None" ] && [ -n "$AMI_ID" ] || { echo "error: could not resolve a DLAMI; set AMI_ID="; exit 1; }

echo "[1b/7] checking $INSTANCE_TYPE availability + picking an AZ/subnet in $REGION"
# Many regions/AZs don't offer a given GPU type; launching into one yields RunInstances 'Unsupported'.
# Find the AZs that DO offer it, then a default subnet in one of them — and fail early (before creating a
# key pair / SG) with an actionable message if none exist.
AZS="$(aws ec2 describe-instance-type-offerings --region "$REGION" --location-type availability-zone \
    --filters "Name=instance-type,Values=$INSTANCE_TYPE" \
    --query 'InstanceTypeOfferings[].Location' --output text)"
if [ -z "$AZS" ]; then
    echo "error: '$INSTANCE_TYPE' is not offered in $REGION."
    echo "  -> retry in a GPU-rich region:   REGION=us-west-2 tools/aws/launch_training.sh"
    echo "                              or:   REGION=us-east-1 tools/aws/launch_training.sh"
    echo "  -> or a widely-available type:    INSTANCE_TYPE=g4dn.xlarge tools/aws/launch_training.sh"
    exit 1
fi
SUBNET="${SUBNET_ID:-}"; AZ_PICK=""
if [ -n "$SUBNET" ]; then
    AZ_PICK="$(aws ec2 describe-subnets --region "$REGION" --subnet-ids "$SUBNET" \
        --query 'Subnets[0].AvailabilityZone' --output text)"
else
    for az in $AZS; do
        s="$(aws ec2 describe-subnets --region "$REGION" \
            --filters "Name=availability-zone,Values=$az" "Name=default-for-az,Values=true" \
            --query 'Subnets[0].SubnetId' --output text 2>/dev/null)"
        [ -n "$s" ] && [ "$s" != None ] && { SUBNET="$s"; AZ_PICK="$az"; break; }
    done
fi
[ -n "$SUBNET" ] && [ "$SUBNET" != None ] || {
    echo "error: no default subnet in an AZ offering $INSTANCE_TYPE ($AZS)."
    echo "  pass an explicit SUBNET_ID=subnet-... in one of those AZs, or try another REGION."; exit 1; }
VPC_ID="$(aws ec2 describe-subnets --region "$REGION" --subnet-ids "$SUBNET" \
    --query 'Subnets[0].VpcId' --output text)"

MYIP="$(curl -fsS https://checkip.amazonaws.com 2>/dev/null | tr -d '[:space:]')/32"
SUFFIX="$(date +%s | tail -c 6)"
KEY_NAME="${KEY_NAME:-pawnstar-train-$SUFFIX}"
SG_NAME="pawnstar-train-sg-$SUFFIX"
PEM="$REPO/tools/aws/$KEY_NAME.pem"

cat <<PLAN

  ============ launch plan ============
  region / AZ     $REGION / $AZ_PICK
  instance type   $INSTANCE_TYPE  $( [ "$SPOT" = 1 ] && echo '(SPOT)' || echo '(on-demand)' )
  AMI             $AMI_ID  (Deep Learning Base, CUDA+driver preinstalled)
  subnet / vpc    $SUBNET / $VPC_ID
  root volume     ${VOLUME_GB} GB gp3
  key pair        $KEY_NAME   (pem -> $PEM)
  SSH ingress     $MYIP only
  repo ref        $GIT_REF  (pushed via scp; no tokens on the instance)
  bootstrap run   ${RUN:-<none — provision + upload only>}
  est. cost       ~\$1/hr on-demand (g5.xlarge); terminate when done (tools/aws/teardown.sh)
  =====================================

PLAN
if [ "${CONFIRM:-0}" != 1 ]; then
    read -r -p "Proceed? [y/N] " ans; [ "$ans" = y ] || { echo "aborted"; exit 1; }
fi

echo "[2/7] key pair + security group"
if ! aws ec2 describe-key-pairs --region "$REGION" --key-names "$KEY_NAME" >/dev/null 2>&1; then
    aws ec2 create-key-pair --region "$REGION" --key-name "$KEY_NAME" \
        --query KeyMaterial --output text > "$PEM"
    chmod 600 "$PEM"
fi
SG_ID="$(aws ec2 create-security-group --region "$REGION" --group-name "$SG_NAME" --vpc-id "$VPC_ID" \
    --description "pawnstar training SSH" \
    --tag-specifications "ResourceType=security-group,Tags=[{Key=Project,Value=pawnstar-nnue}]" \
    --query GroupId --output text)"
aws ec2 authorize-security-group-ingress --region "$REGION" --group-id "$SG_ID" \
    --protocol tcp --port 22 --cidr "$MYIP" >/dev/null

echo "[3/7] launching instance"
MARKET=(); [ "$SPOT" = 1 ] && MARKET=(--instance-market-options 'MarketType=spot')
IID="$(aws ec2 run-instances --region "$REGION" --image-id "$AMI_ID" --instance-type "$INSTANCE_TYPE" \
    --key-name "$KEY_NAME" --security-group-ids "$SG_ID" --subnet-id "$SUBNET" \
    --associate-public-ip-address "${MARKET[@]}" \
    --block-device-mappings "DeviceName=/dev/sda1,Ebs={VolumeSize=$VOLUME_GB,VolumeType=gp3,DeleteOnTermination=true}" \
    --tag-specifications "ResourceType=instance,Tags=[{Key=Project,Value=pawnstar-nnue},{Key=Name,Value=pawnstar-train}]" \
    --query 'Instances[0].InstanceId' --output text)"
echo "    instance $IID — waiting for status ok (a few minutes)…"
aws ec2 wait instance-status-ok --region "$REGION" --instance-ids "$IID"
IP="$(aws ec2 describe-instances --region "$REGION" --instance-ids "$IID" \
    --query 'Reservations[0].Instances[0].PublicIpAddress' --output text)"
SSH=(ssh -i "$PEM" -o StrictHostKeyChecking=accept-new -o ConnectTimeout=10 "$SSH_USER@$IP")

echo "[4/7] waiting for SSH ($IP)"
for i in $(seq 1 30); do "${SSH[@]}" true 2>/dev/null && break || sleep 10; done

echo "[5/7] packaging repo ($GIT_REF) and uploading"
TAR="$(mktemp /tmp/pawnstar.XXXXXX.tar.gz)"
git -C "$REPO" archive --format=tar.gz -o "$TAR" "$GIT_REF"
scp -i "$PEM" -o StrictHostKeyChecking=accept-new "$TAR" "$SSH_USER@$IP:/tmp/pawnstar.tar.gz"
"${SSH[@]}" 'rm -rf ~/pawnstar && mkdir ~/pawnstar && tar -xzf /tmp/pawnstar.tar.gz -C ~/pawnstar'
rm -f "$TAR"

echo "[6/7] bootstrap stages: ${RUN:-none}"
if [ -n "$RUN" ]; then
    "${SSH[@]}" "RUN='$RUN' bash ~/pawnstar/tools/aws/bootstrap_train.sh"
fi

echo "[7/7] ready"
cat <<DONE

  instance:  $IID   ($IP, $REGION)
  ssh:       ssh -i $PEM $SSH_USER@$IP
  on the box, run staged:
     RUN=setup bash ~/pawnstar/tools/aws/bootstrap_train.sh   # CUDA check, Rust, bullet
     RUN=data  SHARDS="11128 12128 12932 13227" bash ~/pawnstar/tools/aws/bootstrap_train.sh
     RUN=train SBS=120 bash ~/pawnstar/tools/aws/bootstrap_train.sh   # -> ~/pawnstar_nnue/out.bin
  fetch the net back:
     scp -i $PEM $SSH_USER@$IP:~/pawnstar_nnue/out.bin ./
  TEAR DOWN when done (stops billing):
     REGION=$REGION IID=$IID KEY_NAME=$KEY_NAME SG_ID=$SG_ID bash tools/aws/teardown.sh

DONE
