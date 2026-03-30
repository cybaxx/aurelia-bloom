# -*- mode: ruby -*-
# vi: set ft=ruby :

# HardenedBSD Cyber Range
# Build the base box first: cd packer && packer build hardenedbsd.pkr.hcl
# Then: vagrant up

BOX_NAME = "hardenedbsd-15stable"

Vagrant.configure("2") do |config|
  config.vm.box = BOX_NAME
  config.vm.box_check_update = false
  config.vm.synced_folder ".", "/vagrant", type: "rsync",
    rsync__exclude: [".git/", "packer/output-hardenedbsd/"]

  config.ssh.shell = "sh"

  # ── Attacker ──────────────────────────────────────────────
  config.vm.define "attacker", primary: true do |m|
    m.vm.hostname = "attacker"
    m.vm.network "private_network", ip: "192.168.56.10"

    m.vm.provider "virtualbox" do |vb|
      vb.memory = 2048
      vb.cpus   = 2
      vb.name   = "cyber-range-attacker"
    end

    m.vm.provision "shell", path: "provisioning/common.sh"
    m.vm.provision "shell", path: "provisioning/attacker.sh"
  end

  # ── Target: Basic (protections disabled) ──────────────────
  config.vm.define "target-basic" do |m|
    m.vm.hostname = "target-basic"
    m.vm.network "private_network", ip: "192.168.56.20"

    m.vm.provider "virtualbox" do |vb|
      vb.memory = 1024
      vb.cpus   = 1
      vb.name   = "cyber-range-target-basic"
    end

    m.vm.provision "shell", path: "provisioning/common.sh"
    m.vm.provision "shell", path: "provisioning/target-basic.sh"
    m.vm.provision "shell", path: "provisioning/lab-setup.sh"
  end

  # ── Target: Hardened (full protections) ───────────────────
  config.vm.define "target-hardened" do |m|
    m.vm.hostname = "target-hardened"
    m.vm.network "private_network", ip: "192.168.56.30"

    m.vm.provider "virtualbox" do |vb|
      vb.memory = 1024
      vb.cpus   = 1
      vb.name   = "cyber-range-target-hardened"
    end

    m.vm.provision "shell", path: "provisioning/common.sh"
    m.vm.provision "shell", path: "provisioning/target-hardened.sh"
    m.vm.provision "shell", path: "provisioning/lab-setup.sh"
  end

  # ── Target: Service (vulnerable services) ─────────────────
  config.vm.define "target-service" do |m|
    m.vm.hostname = "target-service"
    m.vm.network "private_network", ip: "192.168.56.40"

    m.vm.provider "virtualbox" do |vb|
      vb.memory = 1024
      vb.cpus   = 1
      vb.name   = "cyber-range-target-service"
    end

    m.vm.provision "shell", path: "provisioning/common.sh"
    m.vm.provision "shell", path: "provisioning/target-hardened.sh"
    m.vm.provision "shell", path: "provisioning/services.sh"
  end
end
