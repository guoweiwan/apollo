load("//tools:cpplint.bzl", "cpplint")

package(default_visibility = ["//visibility:public"])

cc_library(
    name = "localization_msf_base_map",
    srcs = glob(["*.cc"]),
    hdrs = glob(["*.h"]),
    deps = [
        "//modules/localization/msf/common:localization_msf_common",
        "//third_party/threadpool",
        "@eigen//:eigen",
    ],
)

cpplint()