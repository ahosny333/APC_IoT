set(srcs
  main_project.cpp
  embedded_wifimanager.cpp
  credentials.cpp
  )


idf_component_register(SRCS ${srcs} 
                    INCLUDE_DIRS ".")
