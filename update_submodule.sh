#!/bin/bash
# Update MagLoop_Common_Files submodule to latest version

echo "Updating MagLoop_Common_Files submodule..."

# Update the submodule to latest remote commit
git submodule update --remote MagLoop_Common_Files

# Check if there are changes
if git diff --quiet HEAD -- MagLoop_Common_Files; then
    echo "No updates available for MagLoop_Common_Files"
else
    echo "MagLoop_Common_Files updated to latest version"
    
    # Add and commit the submodule update
    git add MagLoop_Common_Files
    git commit -m "Update MagLoop_Common_Files submodule to latest version"
    
    echo "Changes committed. You can now push with: git push origin master"
fi

echo "Done!"