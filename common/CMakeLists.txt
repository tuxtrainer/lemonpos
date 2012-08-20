#MCH: Deshabilitar debug (para alivianar el ejecutable).
#     cmake -DCMAKE_BUILD_TYPE=release -DCMAKE_INSTALL_PREFIX=/opt/kde4

set(clienteditor.cpp
 )

kde4_add_ui_files( lemon_SRCS
  ui/editclient_widget.ui
  )
