load("//tools:cpplint.bzl", "cpplint")

package(default_visibility = ["//visibility:public"])

cc_library(
    name = "localization_msf_common",
    srcs = [
        "compression.cc",
        "system_utility.cc",
        "timer.cc",
    ],
    hdrs = [
        "compression.h",
        "rect2d.h",
        "system_utility.h",
        "timer.h",
    ],
    deps = [
        "//modules/common:log",
        "@eigen//:eigen",
    ],
)

cpplint()
