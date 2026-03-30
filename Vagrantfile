# -*- mode: ruby -*-
# vi: set ft=ruby :

# HardenedBSD Cyber Range — Single VM + Jails Architecture
# Build the base box first: cd packer && packer build hardenedbsd.pkr.hcl
# Then: vagrant up
#
# Jails provide the attacker/target separation:
#   attacker  = 192.168.56.10  (full userland, exploit tools)
#   basic     = 192.168.56.20  (BusyBox, protections disabled)
#   hardened  = 192.168.56.30  (BusyBox, full protections)
#   service   = 192.168.56.40  (BusyBox + nginx)
#
# SSH into jails: ssh labuser@192.168.56.{10,20,30,40}
# Legacy multi-VM config: see Vagrantfile.multi-vm

BOX_NAME = "hardenedbsd-15stable"

Vagrant.configure("2") do |config|
  config.vm.box = BOX_NAME
  config.vm.box_check_update = false
  config.vm.synced_folder ".", "/vagrant", type: "rsync",
    rsync__exclude: [".git/", "packer/output-hardenedbsd/", ".vagrant/"]

  config.ssh.shell = "sh"
  config.vm.hostname = "cyber-range"

  # Host VM gets bridge IP .1 — jails get .10/.20/.30/.40
  config.vm.network "private_network", ip: "192.168.56.1"

  config.vm.provider "virtualbox" do |vb|
    vb.memory = 2048
    vb.cpus   = 2
    vb.name   = "cyber-range"
    # Enable promiscuous mode for bridge networking to jails
    vb.customize ["modifyvm", :id, "--nicpromisc2", "allow-all"]
  end

  # Provisioner chain: host setup → jails → per-jail config
  config.vm.provision "shell", path: "provisioning/common.sh"
  config.vm.provision "shell", path: "provisioning/jails.sh"
  config.vm.provision "shell", path: "provisioning/jail-attacker.sh"
  config.vm.provision "shell", path: "provisioning/jail-targets.sh"
  config.vm.provision "shell", path: "provisioning/jail-services.sh"
end
