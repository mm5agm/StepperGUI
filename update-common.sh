#!/bin/bash
# Shell script to update MagLoop_Common_Files submodule
# Usage: ./update-common.sh

set -e  # Exit on any error

echo "=== MagLoop Common Files Submodule Updater ==="
echo "Updating MagLoop_Common_Files submodule..."

# Check if we're in a git repository
if [ ! -d ".git" ]; then
    echo "Error: Not in a Git repository!"
    exit 1
fi

# Update submodule to latest
echo "Fetching latest changes..."
git submodule update --remote --merge

# Check if there are changes
if git status --porcelain MagLoop_Common_Files | grep -q "M "; then
    echo "Submodule updated! Committing changes..."
    
    git add MagLoop_Common_Files
    
    commit_msg="Update MagLoop_Common_Files submodule to latest version - $(date '+%Y-%m-%d %H:%M')"
    git commit -m "$commit_msg"
    
    echo "Pushing changes..."
    git push
    
    echo "✓ Submodule update committed and pushed successfully!"
else
    echo "✓ Submodule already up to date."
fi

echo "=== Update Complete ==="