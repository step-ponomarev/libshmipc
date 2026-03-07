 #  --library shmipc_shared may be useful

jextract \
  --include-dir ../include --output src/main/java/ \
  --target-package jextract \
  ../include/shmipc/ipc_channel.h # and ipc_status.h/ ipc_error.h
