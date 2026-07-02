#!/bin/sh
set -xe
ARTIFACT_NAME="$1"

ARTIFACT_ID=$(gh api repos/$GITHUB_REPOSITORY/actions/runs/$GITHUB_RUN_ID/artifacts --jq ".artifacts[] | select(.name==\"$ARTIFACT_NAME\") | .id")
gh api -X DELETE repos/$GITHUB_REPOSITORY/actions/artifacts/$ARTIFACT_ID
echo "Deleted artifact $ARTIFACT_NAME with ID $ARTIFACT_ID"
