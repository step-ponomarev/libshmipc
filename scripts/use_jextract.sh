jextract \
  --include-dir ../include --output src/main/java/ \
  --target-package jextract \
  --library shmipc_shared \
  ../include/shmipc/ipc_channel.h # and ipc_common.h