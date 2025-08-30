set(srcs
  main_project.cpp
  embedded_wifimanager.cpp
  credentials.cpp
  app_webserver_idf.cpp
  MQTT.cpp
  dse_modbus.cpp
  )

idf_component_register(SRCS ${srcs} 
                    INCLUDE_DIRS "."
                    EMBED_FILES "web/ota.html" "web/index.html" "web/wifimanager.html" "web/home.html")