# M5 Protocol Integration Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Integrate M1-M4 modules into a complete PQ-GM-IKEv2 protocol flow with triple key exchange (x25519 + ML-KEM + SM2-KEM).

**Architecture:** Use strongSwan's RFC 9370 ADDKE mechanism for automatic key exchange sequencing. Certificate distribution in IKE_INTERMEDIATE #0 is triggered by message ID check. Key derivation follows RFC 9370 PRF chain.

**Tech Stack:** strongSwan 6.0.4, GmSSL 3.1.1, gmalg plugin, ml plugin, swanctl

---

## Phase 1: Configuration and Code Modification

### Task 1: Create Initiator swanctl.conf

**Files:**
- Create: `/home/ipsec/PQGM-IPSec/configs/initiator/swanctl.conf`

**Step 1: Create configs directory**

```bash
mkdir -p /home/ipsec/PQGM-IPSec/configs/initiator
mkdir -p /home/ipsec/PQGM-IPSec/configs/responder
```

**Step 2: Write initiator configuration**

Create file `/home/ipsec/PQGM-IPSec/configs/initiator/swanctl.conf`:

```conf
# PQ-GM-IKEv2 Initiator Configuration
# Triple Key Exchange: x25519 + ML-KEM-768 + SM2-KEM

connections {
    pqgm-ikev2 {
        version = 2
        local_addrs = 192.168.1.10
        remote_addrs = 192.168.1.20

        # IKE SA proposals with triple key exchange
        # KE=x25519, ke1_=ml-kem-768, ke2_=sm2-kem
        proposals = aes256-sha384-x25519-ke1_mlkem768-ke2_sm2kem

        local {
            auth = pubkey
            certs = sign_cert.pem
            id = initiator.pqgm.test
        }

        remote {
            auth = pubkey
            id = responder.pqgm.test
        }

        children {
            ipsec {
                local_ts = 10.1.0.0/16
                remote_ts = 10.2.0.0/16

                # ESP proposals with triple key exchange
                esp_proposals = aes256gcm256-x25519-ke1_mlkem768-ke2_sm2kem

                updown = /usr/local/libexec/ipsec/_updown iptables
            }
        }
    }
}

secrets {
    # SM2 signing key
    sm2-sign {
        file = sign_key.pem
        secret = "PQGM2026"
    }
    # SM2 encryption key (for SM2-KEM)
    sm2-enc {
        file = enc_key.pem
        secret = "PQGM2026"
    }
}
```

**Step 3: Commit configuration**

```bash
cd /home/ipsec/PQGM-IPSec
git add configs/initiator/swanctl.conf
git commit -m "feat(config): add initiator swanctl.conf with triple KE"
```

---

### Task 2: Create Responder swanctl.conf

**Files:**
- Create: `/home/ipsec/PQGM-IPSec/configs/responder/swanctl.conf`

**Step 1: Write responder configuration**

Create file `/home/ipsec/PQGM-IPSec/configs/responder/swanctl.conf`:

```conf
# PQ-GM-IKEv2 Responder Configuration
# Triple Key Exchange: x25519 + ML-KEM-768 + SM2-KEM

connections {
    pqgm-ikev2 {
        version = 2
        local_addrs = 192.168.1.20

        # IKE SA proposals with triple key exchange
        proposals = aes256-sha384-x25519-ke1_mlkem768-ke2_sm2kem

        local {
            auth = pubkey
            certs = sign_cert.pem
            id = responder.pqgm.test
        }

        remote {
            auth = pubkey
            id = initiator.pqgm.test
        }

        children {
            ipsec {
                local_ts = 10.2.0.0/16
                remote_ts = 10.1.0.0/16

                # ESP proposals with triple key exchange
                esp_proposals = aes256gcm256-x25519-ke1_mlkem768-ke2_sm2kem

                updown = /usr/local/libexec/ipsec/_updown iptables
            }
        }
    }
}

secrets {
    # SM2 signing key
    sm2-sign {
        file = sign_key.pem
        secret = "PQGM2026"
    }
    # SM2 encryption key (for SM2-KEM)
    sm2-enc {
        file = enc_key.pem
        secret = "PQGM2026"
    }
}
```

**Step 2: Commit configuration**

```bash
cd /home/ipsec/PQGM-IPSec
git add configs/responder/swanctl.conf
git commit -m "feat(config): add responder swanctl.conf with triple KE"
```

---

### Task 3: Add Message ID Check to ike_cert_post.c

**Files:**
- Modify: `/home/ipsec/strongswan/src/libcharon/sa/ikev2/tasks/ike_cert_post.c:507-545`

**Step 1: Backup original file**

```bash
cp /home/ipsec/strongswan/src/libcharon/sa/ikev2/tasks/ike_cert_post.c \
   /home/ipsec/strongswan/src/libcharon/sa/ikev2/tasks/ike_cert_post.c.bak
```

**Step 2: Modify should_send_intermediate_certs() function**

Locate the function `should_send_intermediate_certs()` (around line 507) and add message ID check:

```c
/**
 * Check if we should send certificates in IKE_INTERMEDIATE
 */
static bool should_send_intermediate_certs(private_ike_cert_post_t *this,
                                           message_t *message)
{
    /* Only send once */
    if (this->intermediate_certs_sent)
    {
        return FALSE;
    }

    /* PQ-GM-IKEv2: Only send in the first IKE_INTERMEDIATE message (message_id == 1)
     * IKE_SA_INIT uses message_id 0, first IKE_INTERMEDIATE uses message_id 1 */
    if (message->get_message_id(message) != 1)
    {
        DBG2(DBG_IKE, "PQ-GM-IKEv2: not first IKE_INTERMEDIATE (mid=%d), "
             "skipping cert distribution", message->get_message_id(message));
        return FALSE;
    }

    /* Check if peer supports IKE_INTERMEDIATE */
    if (!this->ike_sa->supports_extension(this->ike_sa, EXT_IKE_INTERMEDIATE))
    {
        return FALSE;
    }

    /* Check certificate policy */
    peer_cfg_t *peer_cfg;
    peer_cfg = this->ike_sa->get_peer_cfg(this->ike_sa);
    if (!peer_cfg)
    {
        return FALSE;
    }

    switch (peer_cfg->get_cert_policy(peer_cfg))
    {
        case CERT_NEVER_SEND:
            return FALSE;
        case CERT_SEND_IF_ASKED:
            if (!this->ike_sa->has_condition(this->ike_sa, COND_CERTREQ_SEEN))
            {
                return FALSE;
            }
            /* FALL */
        case CERT_ALWAYS_SEND:
            DBG1(DBG_IKE, "PQ-GM-IKEv2: will send certificates in "
                 "IKE_INTERMEDIATE #0 (mid=1)");
            return TRUE;
    }

    return FALSE;
}
```

**Step 3: Verify modification**

```bash
grep -n "get_message_id" /home/ipsec/strongswan/src/libcharon/sa/ikev2/tasks/ike_cert_post.c
```

Expected output: Should show the new message_id check.

**Step 4: Commit modification**

```bash
cd /home/ipsec/strongswan
git add src/libcharon/sa/ikev2/tasks/ike_cert_post.c
git commit -m "feat(ike): add message ID check for IKE_INTERMEDIATE cert distribution

PQ-GM-IKEv2: Only send certificates in the first IKE_INTERMEDIATE
message (message_id == 1), ensuring proper certificate exchange
before ADDKE key exchanges."
```

---

### Task 4: Rebuild strongSwan

**Files:**
- Build: strongSwan with gmalg plugin

**Step 1: Clean previous build (optional)**

```bash
cd /home/ipsec/strongswan
make clean 2>/dev/null || true
```

**Step 2: Configure with gmalg plugin**

```bash
cd /home/ipsec/strongswan
./configure --enable-gmalg --enable-swanctl --with-gmssl=/usr/local \
    --enable-ml --enable-openssl --enable-pem --enable-pkcs1 \
    --enable-pubkey --enable-x509 --enable-eap-identity
```

Expected output: Configuration should complete without errors.

**Step 3: Build**

```bash
cd /home/ipsec/strongswan
make -j$(nproc)
```

Expected output: Build should complete without errors.

**Step 4: Install**

```bash
cd /home/ipsec/strongswan
echo "1574a" | sudo -S make install
```

Expected output: Installation should complete without errors.

**Step 5: Verify installation**

```bash
swanctl --version
charon-cmd --version
```

Expected output: Should show strongSwan version 6.0.x.

---

## Phase 2: Certificate and Key Setup

### Task 5: Copy Certificates to Config Directories

**Files:**
- Copy: From `certs/` to `configs/initiator/` and `configs/responder/`

**Step 1: Create certificate directories**

```bash
mkdir -p /home/ipsec/PQGM-IPSec/configs/initiator/x509
mkdir -p /home/ipsec/PQGM-IPSec/configs/initiator/x509ca
mkdir -p /home/ipsec/PQGM-IPSec/configs/initiator/private

mkdir -p /home/ipsec/PQGM-IPSec/configs/responder/x509
mkdir -p /home/ipsec/PQGM-IPSec/configs/responder/x509ca
mkdir -p /home/ipsec/PQGM-IPSec/configs/responder/private
```

**Step 2: Copy initiator certificates**

```bash
cd /home/ipsec/PQGM-IPSec

# Copy CA certificate
cp certs/ca/ca_sm2_cert.pem configs/initiator/x509ca/

# Copy initiator certificates
cp certs/initiator/sign_cert.pem configs/initiator/x509/
cp certs/initiator/enc_cert.pem configs/initiator/x509/

# Copy initiator private keys
cp certs/initiator/sign_key.pem configs/initiator/private/
cp certs/initiator/enc_key.pem configs/initiator/private/
```

**Step 3: Copy responder certificates**

```bash
cd /home/ipsec/PQGM-IPSec

# Copy CA certificate
cp certs/ca/ca_sm2_cert.pem configs/responder/x509ca/

# Copy responder certificates
cp certs/responder/sign_cert.pem configs/responder/x509/
cp certs/responder/enc_cert.pem configs/responder/x509/

# Copy responder private keys
cp certs/responder/sign_key.pem configs/responder/private/
cp certs/responder/enc_key.pem configs/responder/private/
```

**Step 4: Verify certificate structure**

```bash
ls -la /home/ipsec/PQGM-IPSec/configs/initiator/x509/
ls -la /home/ipsec/PQGM-IPSec/configs/responder/x509/
```

Expected output: Should show sign_cert.pem and enc_cert.pem in both directories.

**Step 5: Commit**

```bash
cd /home/ipsec/PQGM-IPSec
git add configs/
git commit -m "feat(config): add certificates to initiator/responder configs"
```

---

### Task 6: Verify SM2-KEM Transform ID Registration

**Files:**
- Verify: `/home/ipsec/strongswan/src/libstrongswan/plugins/gmalg/gmalg_plugin.c`

**Step 1: Check plugin registration**

```bash
grep -A5 "sm2.*kem\|KE_SM2\|1051" /home/ipsec/strongswan/src/libstrongswan/plugins/gmalg/gmalg_plugin.c
```

Expected output: Should show SM2-KEM registration with Transform ID 1051 or custom ID.

**Step 2: Verify transform definition**

```bash
grep -n "sm2kem\|SM2_KEM\|KE_SM2" /home/ipsec/strongswan/src/libstrongswan/crypto/transform.h
```

Expected output: Should show SM2-KEM transform definition.

---

## Phase 3: Testing and Verification

### Task 7: Create End-to-End Test Script

**Files:**
- Create: `/home/ipsec/PQGM-IPSec/scripts/test_pqgm_ikev2.sh`

**Step 1: Create test script**

Create file `/home/ipsec/PQGM-IPSec/scripts/test_pqgm_ikev2.sh`:

```bash
#!/bin/bash
# PQ-GM-IKEv2 End-to-End Test Script

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"

echo "========================================="
echo "  PQ-GM-IKEv2 End-to-End Test"
echo "========================================="

# Check if running as root
if [ "$EUID" -ne 0 ]; then
    echo "Please run as root"
    exit 1
fi

# Function to check strongSwan status
check_status() {
    echo "Checking strongSwan status..."
    swanctl --list-sas
}

# Function to start strongSwan
start_strongswan() {
    echo "Starting strongSwan..."
    systemctl start strongswan-starter || charon-cmd --debug-all
}

# Function to stop strongSwan
stop_strongswan() {
    echo "Stopping strongSwan..."
    systemctl stop strongswan-starter 2>/dev/null || pkill charon
}

# Function to test IKE_SA_INIT
test_ike_init() {
    echo ""
    echo "=== Testing IKE_SA_INIT ==="

    # Check journal for IKE_SA_INIT messages
    journalctl -u strongswan-starter --since "1 minute ago" | grep -i "IKE_SA_INIT" || true

    echo "Checking for triple KE negotiation..."
    journalctl -u strongswan-starter --since "1 minute ago" | grep -i "ke1_mlkem\|ke2_sm2" || true
}

# Function to test IKE_INTERMEDIATE
test_ike_intermediate() {
    echo ""
    echo "=== Testing IKE_INTERMEDIATE ==="

    echo "Checking for certificate distribution..."
    journalctl -u strongswan-starter --since "1 minute ago" | grep -i "PQ-GM-IKEv2.*cert" || true

    echo "Checking for ADDKE execution..."
    journalctl -u strongswan-starter --since "1 minute ago" | grep -i "ADDKE\|additional.*key" || true
}

# Function to test IPsec SA
test_ipsec_sa() {
    echo ""
    echo "=== Testing IPsec SA ==="

    echo "Listing IPsec SAs..."
    ip xfrm state || true

    echo "Listing IPsec policies..."
    ip xfrm policy || true
}

# Function to test connectivity
test_connectivity() {
    echo ""
    echo "=== Testing Connectivity ==="

    if [ -n "$REMOTE_IP" ]; then
        echo "Pinging remote..."
        ping -c 3 "$REMOTE_IP" || true
    else
        echo "REMOTE_IP not set, skipping ping test"
    fi
}

# Main test flow
main() {
    echo ""
    echo "1. Checking prerequisites..."

    # Check if certificates exist
    if [ ! -f "$PROJECT_DIR/configs/initiator/x509/sign_cert.pem" ]; then
        echo "ERROR: Initiator certificates not found"
        exit 1
    fi

    echo "Certificates found: OK"

    # Check if strongSwan is installed
    if ! command -v swanctl &> /dev/null; then
        echo "ERROR: swanctl not found"
        exit 1
    fi

    echo "strongSwan installed: OK"

    echo ""
    echo "2. Starting test..."

    # Run tests based on arguments
    case "${1:-all}" in
        "init")
            test_ike_init
            ;;
        "intermediate")
            test_ike_intermediate
            ;;
        "ipsec")
            test_ipsec_sa
            ;;
        "connectivity")
            test_connectivity
            ;;
        "all")
            test_ike_init
            test_ike_intermediate
            test_ipsec_sa
            test_connectivity
            ;;
        *)
            echo "Unknown test: $1"
            echo "Usage: $0 [init|intermediate|ipsec|connectivity|all]"
            exit 1
            ;;
    esac

    echo ""
    echo "========================================="
    echo "  Test Complete"
    echo "========================================="
}

main "$@"
```

**Step 2: Make script executable**

```bash
chmod +x /home/ipsec/PQGM-IPSec/scripts/test_pqgm_ikev2.sh
```

**Step 3: Commit**

```bash
cd /home/ipsec/PQGM-IPSec
git add scripts/test_pqgm_ikev2.sh
git commit -m "feat(test): add end-to-end test script for PQ-GM-IKEv2"
```

---

### Task 8: Create Performance Benchmark Script

**Files:**
- Create: `/home/ipsec/PQGM-IPSec/scripts/benchmark_pqgm.sh`

**Step 1: Create benchmark script**

Create file `/home/ipsec/PQGM-IPSec/scripts/benchmark_pqgm.sh`:

```bash
#!/bin/bash
# PQ-GM-IKEv2 Performance Benchmark Script

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
RESULTS_DIR="$SCRIPT_DIR/../results"
TIMESTAMP=$(date +%Y%m%d_%H%M%S)

mkdir -p "$RESULTS_DIR"

echo "========================================="
echo "  PQ-GM-IKEv2 Performance Benchmark"
echo "  Timestamp: $TIMESTAMP"
echo "========================================="

# Test 1: IKE SA establishment time
benchmark_ike_establishment() {
    echo ""
    echo "=== IKE SA Establishment Time ==="

    local start_time end_time duration

    start_time=$(date +%s%N)

    # Trigger IKE negotiation
    swanctl --initiate --child ipsec 2>&1 | tee "$RESULTS_DIR/ike_init_$TIMESTAMP.log"

    end_time=$(date +%s%N)

    duration=$(( (end_time - start_time) / 1000000 ))

    echo "IKE SA establishment time: ${duration}ms"

    echo "${TIMESTAMP},ike_establishment,${duration}" >> "$RESULTS_DIR/benchmark_results.csv"
}

# Test 2: Data throughput
benchmark_throughput() {
    echo ""
    echo "=== Data Throughput ==="

    if command -v iperf3 &> /dev/null; then
        echo "Running iperf3 test..."
        iperf3 -c "$REMOTE_IP" -t 10 -i 1 | tee "$RESULTS_DIR/iperf3_$TIMESTAMP.log"
    else
        echo "iperf3 not installed, skipping throughput test"
    fi
}

# Test 3: CPU and memory usage
benchmark_resources() {
    echo ""
    echo "=== Resource Usage ==="

    echo "CPU usage:"
    top -bn1 | grep charon || true

    echo "Memory usage:"
    ps aux | grep charon | awk '{print $6}' || true
}

# Test 4: Packet size analysis
benchmark_packet_sizes() {
    echo ""
    echo "=== Packet Size Analysis ==="

    echo "Capturing IKE packets..."
    timeout 30 tcpdump -i any port 500 or port 4500 -w "$RESULTS_DIR/ike_capture_$TIMESTAMP.pcap" 2>&1 || true

    echo "Analyzing packet sizes..."
    tcpdump -r "$RESULTS_DIR/ike_capture_$TIMESTAMP.pcap" -nn 2>/dev/null | head -20 || true
}

# Generate summary report
generate_report() {
    echo ""
    echo "========================================="
    echo "  Benchmark Summary"
    echo "========================================="

    cat "$RESULTS_DIR/benchmark_results.csv" 2>/dev/null || echo "No results yet"

    echo ""
    echo "Results saved to: $RESULTS_DIR"
}

# Main
main() {
    echo "Starting benchmark..."

    benchmark_ike_establishment
    benchmark_throughput
    benchmark_resources
    benchmark_packet_sizes

    generate_report
}

main "$@"
```

**Step 2: Make executable**

```bash
chmod +x /home/ipsec/PQGM-IPSec/scripts/benchmark_pqgm.sh
```

**Step 3: Commit**

```bash
cd /home/ipsec/PQGM-IPSec
git add scripts/benchmark_pqgm.sh
git commit -m "feat(test): add performance benchmark script"
```

---

## Phase 4: Documentation

### Task 9: Create Integration Documentation

**Files:**
- Create: `/home/ipsec/PQGM-IPSec/docs/pqgm-ikev2-integration.md`

**Step 1: Create documentation**

Create file `/home/ipsec/PQGM-IPSec/docs/pqgm-ikev2-integration.md`:

```markdown
# PQ-GM-IKEv2 Integration Guide

## Overview

This document describes how to configure and test the PQ-GM-IKEv2 protocol with triple key exchange.

## Prerequisites

- strongSwan 6.0.4+ with gmalg and ml plugins
- GmSSL 3.1.1
- SM2 certificates (sign_cert.pem, enc_cert.pem)

## Configuration

### 1. Directory Structure

```
configs/
├── initiator/
│   ├── swanctl.conf
│   ├── x509/
│   │   ├── sign_cert.pem
│   │   └── enc_cert.pem
│   ├── x509ca/
│   │   └── ca_sm2_cert.pem
│   └── private/
│       ├── sign_key.pem
│       └── enc_key.pem
└── responder/
    └── (same structure)
```

### 2. swanctl.conf Configuration

Key configuration elements:

```conf
proposals = aes256-sha384-x25519-ke1_mlkem768-ke2_sm2kem
```

This configures:
- KE=x25519 (classic DH)
- ke1_=sm2-kem (chinese national KEM, ADDKE1)
- ke2_=ml-kem-768 (post-quantum KEM, ADDKE2)

## Protocol Flow

1. **IKE_SA_INIT**: Negotiate triple KE
2. **IKE_INTERMEDIATE #0**: Exchange dual certificates
3. **IKE_INTERMEDIATE #1**: Execute SM2-KEM
4. **IKE_INTERMEDIATE #2**: Execute ML-KEM-768
5. **IKE_AUTH**: Authenticate with ML-DSA/SPHINCS+

## Testing

```bash
# Run end-to-end test
sudo ./scripts/test_pqgm_ikev2.sh all

# Run benchmark
sudo ./scripts/benchmark_pqgm.sh
```

## Troubleshooting

### Issue: Certificate distribution not happening

Check that:
1. Peer supports IKE_INTERMEDIATE
2. Certificate policy is CERT_ALWAYS_SEND
3. Message ID is 1 (first IKE_INTERMEDIATE)

### Issue: ADDKE not executing

Check that:
1. Proposals include ke1_ and ke2_ transforms
2. ml plugin is loaded
3. gmalg plugin is loaded

### Issue: IPsec SA not established

Check journal logs:
```bash
journalctl -u strongswan-starter -f
```
```

**Step 2: Commit**

```bash
cd /home/ipsec/PQGM-IPSec
git add docs/pqgm-ikev2-integration.md
git commit -m "docs: add PQ-GM-IKEv2 integration guide"
```

---

## Verification Checklist

After completing all tasks, verify:

- [ ] swanctl.conf created for initiator and responder
- [ ] Message ID check added to ike_cert_post.c
- [ ] strongSwan rebuilt and installed
- [ ] Certificates copied to config directories
- [ ] Test scripts created and executable
- [ ] Documentation created

## Next Steps

After successful implementation:

1. Run single-node test to verify configuration syntax
2. Run dual-node test to verify complete protocol flow
3. Collect performance data for thesis
4. Update MODULES.md with completion status
