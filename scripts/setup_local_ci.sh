#!/bin/bash

echo "🔧 Setting up local CI testing tools..."
echo "======================================"

# Install act (GitHub Actions runner for local testing)
echo "📦 Installing act..."
if [[ "$OSTYPE" == "linux-gnu"* ]]; then
    echo "Linux detected - installing act via curl..."
    curl -s https://raw.githubusercontent.com/nektos/act/master/install.sh | sudo bash
elif [[ "$OSTYPE" == "darwin"* ]]; then
    echo "macOS detected - installing act via brew..."
    if ! command -v brew &> /dev/null; then
        echo "❌ Homebrew not found. Please install Homebrew first:"
        echo "   /bin/bash -c \"\$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)\""
        exit 1
    fi
    brew install act
else
    echo "❌ Unsupported OS: $OSTYPE"
    echo "Please install act manually from: https://github.com/nektos/act"
    exit 1
fi

echo ""
echo "✅ act installed successfully!"
echo ""
echo "🚀 Now you can run GitHub Actions locally:"
echo ""
echo "   # Test quick_check workflow (PR tests)"
echo "   act -W .github/workflows/quick_check.yml"
echo ""
echo "   # Test release workflow (release tests)"
echo "   act -W .github/workflows/release.yml"
echo ""
echo "   # List available workflows"
echo "   act -l"
echo ""
echo "ℹ️  Note: act requires Docker to be running"
