#!/bin/bash
# Install SEGGER J-Link tools on macOS
# This script will install J-Link using Homebrew (requires admin password)

echo "============================================"
echo "Installing SEGGER J-Link Software"
echo "============================================"
echo ""
echo "This will install J-Link using Homebrew."
echo "You will be prompted for your admin password."
echo ""

# Check if Homebrew is installed
if ! command -v brew &> /dev/null; then
    echo "Error: Homebrew is not installed!"
    echo "Please install Homebrew first: https://brew.sh"
    exit 1
fi

# Install J-Link
echo "Installing segger-jlink cask..."
if brew install --cask segger-jlink; then
    echo ""
    echo "✓ J-Link installed successfully!"
    echo ""
    
    # Find JLinkExe
    JLINKEXE=""
    if [ -f "/Applications/SEGGER/JLink/JLinkExe" ]; then
        JLINKEXE="/Applications/SEGGER/JLink/JLinkExe"
    elif command -v JLinkExe &> /dev/null; then
        JLINKEXE=$(command -v JLinkExe)
    fi
    
    if [ -n "$JLINKEXE" ]; then
        echo "J-Link found at: $JLINKEXE"
        echo ""
        
        # Check if it's in PATH
        if ! command -v JLinkExe &> /dev/null; then
            echo "Adding J-Link to PATH..."
            # Add to ~/.zshrc (since user is on zsh)
            if ! grep -q "SEGGER/JLink" ~/.zshrc 2>/dev/null; then
                echo 'export PATH="/Applications/SEGGER/JLink:$PATH"' >> ~/.zshrc
                echo "Added to ~/.zshrc"
            fi
            echo ""
            echo "To use J-Link in this terminal, run:"
            echo "  export PATH=\"/Applications/SEGGER/JLink:\$PATH\""
            echo ""
            echo "Or open a new terminal window."
        fi
        
        echo "Testing J-Link installation..."
        "$JLINKEXE" -? 2>&1 | head -5
        echo ""
        echo "✓ J-Link is ready to use!"
    else
        echo "Warning: JLinkExe not found after installation"
        echo "Please check: /Applications/SEGGER/JLink/"
    fi
else
    echo ""
    echo "Installation failed. You may need to:"
    echo "  1. Run this script manually: ./install-jlink.sh"
    echo "  2. Or install manually from: https://www.segger.com/downloads/jlink/"
    exit 1
fi
