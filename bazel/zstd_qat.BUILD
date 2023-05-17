load("@rules_cc//cc:defs.bzl", "cc_library")

cc_library(
   name = "lib",
   srcs = ["lib/libzstd.a","lib/libqat_s.so","lib/libusdm_drv_s.so","lib/libqatseqprod.so"],
   hdrs = ["include/qatseqprod.h","include/zstd.h"],
   visibility = ["//visibility:public"],
)
