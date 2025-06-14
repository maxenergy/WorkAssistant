#!/bin/bash

# Function to stop VNC and websockify
stop_vnc() {
    echo "Stopping VNC and websockify processes..."
    pkill -f websockify || true
    vncserver -kill :1 || true
    rm -rf /tmp/.X1-lock /tmp/.X11-unix/X1 2>/dev/null || true
}

# Function to start VNC and websockify
start_vnc() {
    # Get current user
    CURRENT_USER=$(whoami)
    USER_HOME=$(eval echo ~$CURRENT_USER)

    # Setup VNC password if not exists
    if [ ! -f "$USER_HOME/.vnc/passwd" ]; then
        mkdir -p $USER_HOME/.vnc
        echo "password" | vncpasswd -f > $USER_HOME/.vnc/passwd
        chmod 600 $USER_HOME/.vnc/passwd
    fi

    # Create VNC configuration if not exists
    if [ ! -f "$USER_HOME/.vnc/xstartup" ]; then
        cat > $USER_HOME/.vnc/xstartup << EOF
#!/bin/bash
xrdb $USER_HOME/.Xresources

# Set Firefox as default browser
xdg-settings set default-web-browser firefox.desktop

# Start desktop environment
startxfce4 &
EOF
        chmod +x $USER_HOME/.vnc/xstartup
    fi

    # Start VNC server
    vncserver :1 -geometry 1280x800 -depth 24

    # Setup noVNC
    sudo websockify -D --web=/usr/share/novnc/ 6080 localhost:5901

    echo "Desktop environment has been set up!"
    echo "You can access it through noVNC at: http://localhost:6080"
}

# Check command line arguments
case "$1" in
    restart)
        stop_vnc
        start_vnc
        ;;
    stop)
        stop_vnc
        ;;
    start)
        start_vnc
        ;;
    *)
        # First time setup
        # Update package lists
        sudo apt-get update

        # Install Ubuntu desktop, VNC server, and other necessary packages
        sudo DEBIAN_FRONTEND=noninteractive apt-get install -y \
            ubuntu-desktop \
            tightvncserver \
            xfce4 \
            xfce4-goodies \
            novnc \
            net-tools \
            websockify

        start_vnc
        ;;
esac