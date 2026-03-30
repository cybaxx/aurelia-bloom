#!/bin/bash

# Script to Provision aurelia-bloom Environment

# --- Configuration ---
TARGET_USER="aurelia"          # User to create
WORKDIR="/home/$TARGET_USER"  # Working directory
VAGRANT_BOX="hardenedbsd-15stable.box" # Assume box is in /path/to/boxes/
VAGRANT_VM="aurelia-vm"
VAGRANT_NETWORK="default"

# --- Helper Functions ---

function install_dependencies {
  local os=$(uname -s)
  if [[ "$os" == "Darwin" ]]; then
    brew install --cask --verbose openssl
    brew install --cask --verbose python3
    brew install --cask --verbose python3-pip
  elif [[ "$os" == "Linux" ]]; then
    sudo apt-get update
    sudo apt-get install -y openssl python3 python3-pip
  else
    echo "Unsupported operating system: $os"
    exit 1
  fi
}

# --- Main Script ---

echo "Starting Provisioning..."

# 1. Create User
sudo useradd -m -s /bin/bash $TARGET_USER

# 2. Install Dependencies
install_dependencies

# 3. Provisioning Scripts
echo "Running Provisioning Scripts..."
#  (Replace this with the actual command to run your provisioning scripts)
#  This is just a placeholder - adapt it to your specific needs.
#  Example: ./provisioning_scripts.sh
#  ./provisioning_scripts.sh

# 4. Vagrant Setup
echo "Setting up Vagrant..."
vagrant -v
vagrant box add hardenedbsd-15stable hardenedbsd-15stable.box
vagrant up --provisioner homestead2 --vagrant-env VAGRANT_NETWORK=$VAGRANT_NETWORK
#Or if you prefer to name the VM:
#vagrant up -v $VAGRANT_VM --provisioner homestead2 --vagrant-env VAGRANT_NETWORK=$VAGRANT_NETWORK

# 5.  Player Chat Setup
echo "Setting up Player Chat (Requires manual steps - see README)"

# 6. Finish
echo "Provisioning Complete!"
echo "Vagrant VM: $VAGRANT_VM"
echo "Access the VM via: ssh $TARGET_USER@localhost"

