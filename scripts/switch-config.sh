#!/bin/bash
# Switch between loopback and Docker configurations

SCRIPT_DIR=$(dirname "$0")
PROJECT_DIR=$(realpath "$SCRIPT_DIR/..")

case "$1" in
    loopback|local)
        echo "Switching to loopback configuration..."
        cp "$PROJECT_DIR/configs/loopback/swanctl.conf" /usr/local/etc/swanctl/swanctl.conf
        echo "Done. Loopback config installed to /usr/local/etc/swanctl/swanctl.conf"
        ;;
    docker)
        echo "Updating Docker configurations..."
        cp "$PROJECT_DIR/configs/docker-initiator/swanctl.conf" "$PROJECT_DIR/docker/initiator/config/swanctl.conf"
        cp "$PROJECT_DIR/configs/docker-responder/swanctl.conf" "$PROJECT_DIR/docker/responder/config/swanctl.conf"
        echo "Done. Docker configs updated."
        echo ""
        echo "To test with Docker:"
        echo "  cd $PROJECT_DIR/docker"
        echo "  sudo docker-compose up -d"
        echo "  sudo docker exec pqgm-initiator /usr/local/libexec/ipsec/charon &"
        echo "  sudo docker exec pqgm-responder /usr/local/libexec/ipsec/charon &"
        echo "  # Wait for charon to start"
        echo "  sudo docker exec pqgm-initiator /usr/local/sbin/swanctl --load-all"
        echo "  sudo docker exec pqgm-responder /usr/local/sbin/swanctl --load-all"
        echo "  sudo docker exec pqgm-initiator /usr/local/sbin/swanctl --initiate --child ipsec"
        ;;
    *)
        echo "Usage: $0 {loopback|docker}"
        echo ""
        echo "  loopback - Install local loopback config to /usr/local/etc/swanctl/"
        echo "  docker   - Update Docker initiator/responder configs"
        exit 1
        ;;
esac
