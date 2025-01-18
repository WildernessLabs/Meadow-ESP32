appname := Bluetooth

CC=gcc
CXX=g++
RM=rm -f
CXXFLAGS=--std=c++17 -Wall -Wextra -pedantic -g

BLUETOOTH_DIR := ../components/F7Services/Bluetooth

srcfiles += $(shell find . -name "*.cpp")
srcfiles += $(BLUETOOTH_DIR)/BluetoothAttribute.cpp $(BLUETOOTH_DIR)/BluetoothCharacteristic.cpp 
srcfiles += $(BLUETOOTH_DIR)/BluetoothCharacteristicValue.cpp $(BLUETOOTH_DIR)/BluetoothHandleManager.cpp
srcfiles += $(BLUETOOTH_DIR)/BluetoothService.cpp $(BLUETOOTH_DIR)/BluetoothServiceCollection.cpp $(BLUETOOTH_DIR)/BluetoothManager.cpp
srcfiles += $(BLUETOOTH_DIR)/Uuid.cpp

objects  := $(patsubst %.cpp, %.o, $(srcfiles))

INCLUDES := -I. -I../components/rapidjson -I$(BLUETOOTH_DIR) -I ../components/F7Services/test

DEFINES := -DDESKTOP_BUILD

all: $(appname)

$(appname): $(objects)
	$(CXX) $(CXXFLAGS) $(INCLUDES) $(DEFINES) -o $(appname) $(objects)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) $(DEFINES) -c $< -o $*.o

clean:
	$(RM) -f $(objects) $(appname)
