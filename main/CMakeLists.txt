set(srcs
  main_project.cpp
  embedded_wifimanager.cpp
  credentials.cpp
  app_webserver_idf.cpp
  )

idf_component_register(SRCS ${srcs} 
                    INCLUDE_DIRS "."
                    EMBED_FILES "web/ota.html" "web/index.html")