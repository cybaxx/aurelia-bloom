packer {
  required_plugins {
    virtualbox = {
      version = "~> 1"
      source  = "github.com/hashicorp/virtualbox"
    }
    vagrant = {
      version = "~> 1"
      source  = "github.com/hashicorp/vagrant"
    }
    qemu = {
      version = "~> 1"
      source  = "github.com/hashicorp/qemu"
    }
  }
}

variable "iso_url" {
  type    = string
  default = "https://installers.hardenedbsd.org/pub/15-stable/amd64/amd64/disc1.iso"
}

variable "iso_checksum" {
  type        = string
  default     = "none"
  description = "Set to the SHA-256 of the ISO once known. Use 'none' to skip verification during initial setup."
}

variable "disk_size" {
  type    = number
  default = 6144
}

variable "memory" {
  type    = number
  default = 2048
}

variable "cpus" {
  type    = number
  default = 2
}

source "virtualbox-iso" "hardenedbsd" {
  guest_os_type    = "FreeBSD_64"
  iso_url          = var.iso_url
  iso_checksum     = var.iso_checksum
  disk_size        = var.disk_size
  hard_drive_interface = "sata"

  vm_name          = "hardenedbsd-15stable"
  headless         = true

  vboxmanage = [
    ["modifyvm", "{{.Name}}", "--memory", "${var.memory}"],
    ["modifyvm", "{{.Name}}", "--cpus", "${var.cpus}"],
    ["modifyvm", "{{.Name}}", "--nat-localhostreachable1", "on"],
  ]

  http_directory   = "http"

  boot_wait        = "10s"
  boot_command = [
    "<esc><wait>",
    "boot -s<enter><wait10>",
    "/bin/sh<enter><wait>",
    "mdmfs -s 100m md1 /tmp<enter><wait>",
    "dhclient em0<enter><wait5>",
    "fetch -o /tmp/installerconfig http://{{ .HTTPIP }}:{{ .HTTPPort }}/installerconfig<enter><wait3>",
    "bsdinstall script /tmp/installerconfig<enter>",
  ]

  ssh_username     = "root"
  ssh_password     = "vagrant"
  ssh_port         = 22
  ssh_wait_timeout = "30m"

  shutdown_command  = "poweroff"

  post_shutdown_delay = "10s"

  output_directory  = "output-hardenedbsd"
}

source "qemu" "hardenedbsd" {
  iso_url          = var.iso_url
  iso_checksum     = var.iso_checksum
  disk_size        = "${var.disk_size}M"
  format           = "qcow2"

  vm_name          = "hardenedbsd-15stable"
  headless         = true
  accelerator      = "auto"

  memory           = var.memory
  cpus             = var.cpus
  disk_interface   = "virtio"
  net_device       = "virtio-net"

  http_directory   = "http"

  boot_wait        = "10s"
  boot_command = [
    "<esc><wait>",
    "boot -s<enter><wait10>",
    "/bin/sh<enter><wait>",
    "mdmfs -s 100m md1 /tmp<enter><wait>",
    "dhclient vtnet0<enter><wait5>",
    "fetch -o /tmp/installerconfig http://{{ .HTTPIP }}:{{ .HTTPPort }}/installerconfig<enter><wait3>",
    "bsdinstall script /tmp/installerconfig<enter>",
  ]

  ssh_username     = "root"
  ssh_password     = "vagrant"
  ssh_port         = 22
  ssh_wait_timeout = "30m"

  shutdown_command  = "poweroff"

  output_directory  = "output-hardenedbsd-qemu"
}

build {
  sources = [
    "source.virtualbox-iso.hardenedbsd",
    "source.qemu.hardenedbsd",
  ]

  provisioner "shell" {
    scripts = [
      "scripts/base.sh",
    ]
    environment_vars = [
      "ASSUME_ALWAYS_YES=yes",
    ]
  }

  post-processor "vagrant" {
    only                 = ["virtualbox-iso.hardenedbsd"]
    output               = "hardenedbsd-15stable.box"
    vagrantfile_template = ""
  }
}
