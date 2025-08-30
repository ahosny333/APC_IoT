cmake_minimum_required(VERSION 3.5)

set(includedirs
  src/
  )

set(srcs
  src/Modbus.cpp
  src/ModbusRTU.cpp
  )

idf_component_register(SRCS ${srcs} 
                    INCLUDE_DIRS ${includedirs}
                    REQUIRES "arduino" )
project(Modbus)
