#!/usr/bin/env bash
# Tear down a training instance created by launch_training.sh (stops billing). Terminates the instance and
# deletes the key pair + security group it created. Runs locally with your AWS credentials.
#
# Either target one instance explicitly, or sweep everything tagged Project=pawnstar-nnue.
#
# Usage:
#   REGION=us-east-1 IID=i-... [KEY_NAME=... SG_ID=...] tools/aws/teardown.sh   # one instance
#   REGION=us-east-1 ALL=1 tools/aws/teardown.sh                                # all tagged instances
set -euo pipefail
REGION="${REGION:-$(aws configure get region 2>/dev/null || echo us-east-1)}"

if [ "${ALL:-0}" = 1 ]; then
    IIDS="$(aws ec2 describe-instances --region "$REGION" \
        --filters "Name=tag:Project,Values=pawnstar-nnue" "Name=instance-state-name,Values=pending,running,stopped" \
        --query 'Reservations[].Instances[].InstanceId' --output text)"
    [ -n "$IIDS" ] && { echo "terminating: $IIDS"; aws ec2 terminate-instances --region "$REGION" --instance-ids $IIDS >/dev/null; \
        aws ec2 wait instance-terminated --region "$REGION" --instance-ids $IIDS; } || echo "no tagged instances"
    echo "sweeping tagged security groups…"
    for sg in $(aws ec2 describe-security-groups --region "$REGION" \
            --filters "Name=tag:Project,Values=pawnstar-nnue" --query 'SecurityGroups[].GroupId' --output text); do
        aws ec2 delete-security-group --region "$REGION" --group-id "$sg" 2>/dev/null && echo "  deleted $sg" || true
    done
    echo "sweeping pawnstar-train-* key pairs…"
    for kp in $(aws ec2 describe-key-pairs --region "$REGION" \
            --filters "Name=key-name,Values=pawnstar-train-*" --query 'KeyPairs[].KeyName' --output text); do
        aws ec2 delete-key-pair --region "$REGION" --key-name "$kp" && echo "  deleted key pair $kp" || true
    done
    echo "done."
    exit 0
fi

IID="${IID:?set IID=i-... (or ALL=1 to sweep all tagged)}"
echo "terminating $IID…"
aws ec2 terminate-instances --region "$REGION" --instance-ids "$IID" >/dev/null
aws ec2 wait instance-terminated --region "$REGION" --instance-ids "$IID"
[ -n "${SG_ID:-}" ] && { aws ec2 delete-security-group --region "$REGION" --group-id "$SG_ID" 2>/dev/null && echo "deleted SG $SG_ID" || echo "SG $SG_ID not deleted (in use?)"; }
[ -n "${KEY_NAME:-}" ] && { aws ec2 delete-key-pair --region "$REGION" --key-name "$KEY_NAME" && echo "deleted key pair $KEY_NAME"; }
echo "done — $IID terminated."
