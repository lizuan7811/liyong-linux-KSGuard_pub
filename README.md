# liyong-linux-KSGuard_pub
Linux kernel security monitoring framework with kernel module sensors and kernel-to-userspace event processing.

**Language:** C
**Platform:** Linux
**Technology:** Linux Kernel Module, Character Device, Kernel Events, Userspace Agent
**Status:** Public Portfolio Version

> **Note:** This repository is a public portfolio version of the project. Some implementation details and environment-specific configurations have been omitted to protect security and confidentiality.

---

## Project Overview

`ksm` (Kernel Security Monitor) is a Linux kernel-based security monitoring framework designed to observe system activities from kernel space and deliver security events to a userspace monitoring agent.

The project focuses on the communication and event-processing pipeline between the Linux kernel and userspace.

The main design goal is to provide a lightweight kernel-level sensor that can collect security-relevant events while keeping higher-level processing in userspace.

---

# Architecture

```text
┌───────────────────────────────┐
│        User Space Agent       │
│                               │
│  Event Processing             │
│  Event Analysis               │
│  Monitoring / Reporting       │
└───────────────┬───────────────┘
                │
                │ Character Device
                │
┌───────────────▼───────────────┐
│        Kernel Module          │
│                               │
│  Event Collection             │
│  Event Filtering              │
│  Kernel/User Communication    │
└───────────────┬───────────────┘
                │
                │
┌───────────────▼───────────────┐
│          Linux Kernel         │
│                               │
│  Process Events               │
│  File Events                  │
│  Network Events               │
│  Kernel State                 │
└───────────────────────────────┘
```

The architecture separates **event collection** from **event processing**.

The kernel module is responsible for collecting relevant kernel events, while the userspace component is responsible for higher-level processing and analysis.

---

# Features

## Kernel Module Sensor

A Linux Kernel Module acts as the low-level security sensor.

Responsibilities include:

* Kernel module initialization and cleanup
* Kernel event monitoring
* Event collection
* Event filtering
* Kernel state inspection
* Communication with userspace

---

## Process Monitoring

The framework can monitor process-related activities.

Examples include:

* Process creation
* Process execution
* Process-related kernel events
* Process metadata collection

Conceptually:

```text
Process Event
      │
      ▼
Kernel Event Handler
      │
      ▼
Security Event
      │
      ▼
Userspace Agent
```

---

## File Activity Monitoring

The framework provides a foundation for monitoring file-related activities.

Potential events include:

* File access
* File creation
* File modification
* File-related kernel events

The kernel module collects the low-level event while userspace is responsible for subsequent processing.

---

## Network Event Monitoring

The framework also provides a foundation for observing network-related kernel events.

The monitoring pipeline can be extended to support:

* Network connection events
* Socket-related events
* Network state changes
* Security-related network activity

---

# Kernel-to-Userspace Event Pipeline

One of the primary goals of the project is establishing a reliable communication path between kernel space and userspace.

```text
Linux Kernel
     │
     ▼
Kernel Event
     │
     ▼
Kernel Module
     │
     ▼
Event Filtering
     │
     ▼
Character Device
     │
     ▼
Userspace Agent
     │
     ▼
Event Processing
     │
     ▼
Security Monitoring
```

This design keeps the kernel-side component focused on event collection and minimizes complex processing inside kernel space.

---

# Character Device Interface

The Kernel Module exposes a character device interface for communication with the userspace agent.

The interface provides a boundary between:

```text
Kernel Space
     │
     │ Character Device
     ▼
User Space
```

The userspace agent can interact with the kernel module through standard device-file operations.

Typical operations include:

* Device initialization
* Device open
* Device read
* Device write / control operations
* Device release

---

# Security Event Model

Events collected by the kernel module can be represented as structured security events.

Conceptually:

```text
Security Event
│
├── Event Type
├── Timestamp
├── Process Information
├── Kernel Context
└── Event-specific Metadata
```

Different event types can be extended without changing the overall userspace processing architecture.

---

# Target Monitoring

The current project focuses on monitoring the following areas:

| Category      | Monitoring Target               |
| ------------- | ------------------------------- |
| Kernel Module | Module loading / lifecycle      |
| Process       | Process creation and execution  |
| Kernel        | Kernel state and events         |
| File          | File-related activities         |
| Network       | Network-related events          |
| Security      | Security-relevant system events |

The monitoring framework is designed to be extensible so additional event sources can be added later.

---

# Kernel Module Lifecycle

The module follows the standard Linux Kernel Module lifecycle:

```text
Module Load
    │
    ▼
Initialization
    │
    ├── Register device
    ├── Initialize event monitoring
    └── Initialize internal state
    │
    ▼
Running
    │
    ├── Collect kernel events
    ├── Process events
    └── Report events
    │
    ▼
Module Unload
    │
    ├── Stop monitoring
    ├── Release resources
    └── Unregister device
```

---

# Module Parameters

The module supports runtime parameters for configuration.

Example:

```bash
sudo insmod ksm.ko param_int=100 param_str="hello_security"
```

The exact parameter names and values may change as the project evolves.

---

# Kernel Module Signing

For systems enforcing kernel module signature verification, the module can be signed using the Linux kernel's `sign-file` utility.

Example:

```bash
sudo /usr/src/linux-headers-$(uname -r)/scripts/sign-file \
    sha256 \
    <private-key> \
    <certificate> \
    ksm.ko
```

> Private keys, certificates, and machine-specific signing configurations are intentionally excluded from this public repository.

---

# Build and Run

Build the kernel module using the provided build configuration:

```bash
make
```

Load the module:

```bash
sudo insmod ksm.ko
```

If module parameters are required:

```bash
sudo insmod ksm.ko param_int=100 param_str="hello_security"
```

Verify that the module has been loaded:

```bash
lsmod | grep ksm
```

Inspect kernel messages:

```bash
dmesg | tail
```

Unload the module:

```bash
sudo rmmod ksm
```

> Build commands may vary depending on the Linux kernel version and distribution.

---

# Project Structure

```text
ksm/
│
├── src/
│   ├── ksm_main.c
│   ├── ksm_procfs.c
│   ├── ksm_thread.c
│   └── ...
│
├── include/
│   └── ...
│
├── Makefile
│
└── README.md
```

The exact source layout may evolve as additional monitoring capabilities are introduced.

---

# Technical Highlights

This project demonstrates practical experience with:

* Linux Kernel Module development
* Linux kernel programming
* Kernel / userspace communication
* Character Device interfaces
* Kernel event monitoring
* Process monitoring
* File activity monitoring
* Network event monitoring
* Kernel module lifecycle
* Kernel module parameters
* Kernel module signing
* Event-driven security monitoring

---

# Design Principles

The project follows several design principles:

### 1. Keep Kernel-Space Processing Lightweight

The kernel module focuses primarily on:

```text
Event Collection
       +
Event Filtering
       +
Event Delivery
```

More complex processing is delegated to userspace.

### 2. Separate Sensor and Analysis

```text
Kernel Module
     │
     │ Sensor
     ▼
Security Event
     │
     ▼
Userspace
     │
     │ Analysis
     ▼
Security Monitoring
```

This separation makes the monitoring framework easier to extend.

### 3. Extensible Event Pipeline

Additional kernel event sources can be integrated without fundamentally changing the userspace architecture.

---

# Future Improvements

Potential future development includes:

* More comprehensive process monitoring
* Extended file-system event monitoring
* Network connection tracking
* Structured event serialization
* Event buffering and batching
* Userspace event filtering
* Event persistence
* Prometheus / Grafana integration
* Security event correlation
* Performance benchmarking
* Integration with eBPF-based sensors

---

# Public Repository Scope

This repository is intended as a technical portfolio demonstrating Linux Kernel Module development and kernel/userspace event architecture.

The public version intentionally excludes:

* Private keys
* Production certificates
* Internal infrastructure configuration
* Environment-specific paths
* Sensitive system information
* Production monitoring configuration
* Real customer or production data

All examples and configurations included in this repository are intended for development, testing, and demonstration purposes.
