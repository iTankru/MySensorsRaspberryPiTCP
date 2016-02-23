##########################################################################
# Configurable options                                                   #
##########################################################################
# Install Base location
PREFIX=/usr/local
# Bin Dir
BINDIR=$(PREFIX)/sbin
##########################################################################
# Please do not change anything below this line                          #
##########################################################################
CC=g++
# get PI Revision from cpuinfo
PIREV := $(shell cat /proc/cpuinfo | grep Revision | cut -f 2 -d ":" | sed -e 's/^[[:space:]]*//' -e 's/[[:space:]]*$$//')
CCFLAGS=-Wall -Ofast -mfpu=vfp -lpthread -g -D__Raspberry_Pi -mfloat-abi=hard -mtune=arm1176jzf-s 

ifeq (${PIREV},$(filter ${PIREV},a01041 a21041))
	# a01041 and a21041 are PI 2 Model B and armv7
	CCFLAGS += -march=armv7-a 
else
	# anything else is armv6
	CCFLAGS += -march=armv6zk
endif

ifeq (${PIREV},$(filter ${PIREV},a01041 a21041 0010))
	# a01041 and a21041 are PI 2 Model B with BPLUS Layout and 0010 is Pi Model B+ with BPLUS Layout
	CCFLAGS += -D__PI_BPLUS
endif 

# define all programs
PROGRAMS = MyGateway MySensor MyMessage PiEEPROMFile
GATEWAY_TCP = PiGatewayTCP

GATEWAY_TCP_SRCS = ${GATEWAY_TCP:=.cpp}
SOURCES = ${PROGRAMS:=.cpp}

GATEWAY_TCP_OBJS = ${GATEWAY_TCP:=.o}
OBJS = ${PROGRAMS:=.o}

GATEWAY_TCP_DEPS = ${GATEWAY_TCP:=.h}
DEPS = ${PROGRAMS:=.h}

RF24H = /usr/local/include/RF24
CINCLUDE=-I. -I${RF24H}


all: ${GATEWAY_TCP} 

%.o: %.cpp %.h ${DEPS}
	${CC} -c -o $@ $< ${CCFLAGS} ${CINCLUDE}


${GATEWAY_TCP}: ${OBJS} ${GATEWAY_TCP_OBJS}
	${CC} -o $@ ${OBJS} ${GATEWAY_TCP_OBJS} ${CCFLAGS} ${CINCLUDE} -lrf24-bcm -lutil


clean:
	rm -rf $(PROGRAMS) $(GATEWAY_TCP) ${OBJS} $(GATEWAY_TCP_OBJS)

install: all install-gatewaytcp install-initscripts

install-gatewaytcp: 
	@echo "Installing ${GATEWAY_TCP} to ${BINDIR}"
	@install -m 0755 ${GATEWAY_TCP} ${BINDIR}
	
install-initscripts:
	@echo "Installing initscripts to /etc/init.d"
	@install -m 0755 initscripts/PiGatewayTCP /etc/init.d
	@echo "Installing syslog config to /etc/rsyslog.d"
	@install -m 0755 initscripts/30-PiGatewayTCP.conf /etc/rsyslog.d
	@service rsyslog restart

	
enable-gwtcp: install
	@update-rc.d PiGatewayTCP defaults
	
remove-gwtcp:
	@update-rc.d -f PiGatewayTCP remove
	
uninstall: remove-gwtcp
	@echo "Stopping daemon PiGatewayTCP (ignore errors)"
	-@service PiGatewayTCP stop
	@echo "removing files"
	rm ${BINDIR}/PiGatewayTCP /etc/init.d/PiGatewayTCP /etc/rsyslog.d/30-PiGatewayTCP.conf

