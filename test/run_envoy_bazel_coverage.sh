#!/bin/bash

set -e -o pipefail

LLVM_VERSION="14.0.0"
CLANG_VERSION=$(clang --version | grep version | sed -e 's/\ *clang version \(.*\)\ */\1/')
LLVM_COV_VERSION=$(llvm-cov --version | grep version | sed -e 's/\ *LLVM version \(.*\)/\1/')
LLVM_PROFDATA_VERSION=$(llvm-profdata show --version | grep version | sed -e 's/\ *LLVM version \(.*\)/\1/')

if [ "${CLANG_VERSION}" != "${LLVM_VERSION}" ]
then
  echo "clang version ${CLANG_VERSION} does not match expected ${LLVM_VERSION}"
  exit 1
fi

if [ "${LLVM_COV_VERSION}" != "${LLVM_VERSION}" ]
then
  echo "llvm-cov version ${LLVM_COV_VERSION} does not match expected ${LLVM_VERSION}"
  exit 1
fi

if [ "${LLVM_PROFDATA_VERSION}" != "${LLVM_VERSION}" ]
then
  echo "llvm-profdata version ${LLVM_PROFDATA_VERSION} does not match expected ${LLVM_VERSION}"
  exit 1
fi

[[ -z "${SRCDIR}" ]] && SRCDIR="${PWD}"
[[ -z "${VALIDATE_COVERAGE}" ]] && VALIDATE_COVERAGE=true
[[ -z "${FUZZ_COVERAGE}" ]] && FUZZ_COVERAGE=false
[[ -z "${COVERAGE_THRESHOLD}" ]] && COVERAGE_THRESHOLD=96.1
COVERAGE_TARGET="${COVERAGE_TARGET:-}"
read -ra BAZEL_BUILD_OPTIONS <<< "${BAZEL_BUILD_OPTIONS:-}"

echo "Starting run_envoy_bazel_coverage.sh..."
echo "    PWD=$(pwd)"
echo "    SRCDIR=${SRCDIR}"
echo "    VALIDATE_COVERAGE=${VALIDATE_COVERAGE}"

# This is the target that will be run to generate coverage data. It can be overridden by consumer
# projects that want to run coverage on a different/combined target.
# Command-line arguments take precedence over ${COVERAGE_TARGET}.
if [[ $# -gt 0 ]]; then
  COVERAGE_TARGETS=("$@")
elif [[ -n "${COVERAGE_TARGET}" ]]; then
  COVERAGE_TARGETS=("${COVERAGE_TARGET}")
else
  COVERAGE_TARGETS=(//test/...)
fi

BAZEL_COVERAGE_OPTIONS=()

if [[ "${FUZZ_COVERAGE}" == "true" ]]; then
    # Filter targets to just fuzz tests.
    _targets=$(bazel query "attr('tags', 'fuzz_target', ${COVERAGE_TARGETS[*]})")
    COVERAGE_TARGETS=()
    while read -r line; do COVERAGE_TARGETS+=("$line"); done \
        <<< "$_targets"
    BAZEL_COVERAGE_OPTIONS+=(
        "--config=fuzz-coverage"
        "--test_tag_filters=-nocoverage")
else
    BAZEL_COVERAGE_OPTIONS+=(
        "--config=test-coverage"
        "--test_tag_filters=-nocoverage,-fuzz_target")
fi

# Output unusually long logs due to trace logging.
BAZEL_COVERAGE_OPTIONS+=("--experimental_ui_max_stdouterr_bytes=80000000")

echo "Running bazel coverage with:"
echo "  Options: ${BAZEL_BUILD_OPTIONS[*]} ${BAZEL_COVERAGE_OPTIONS[*]}"
echo "  Targets: ${COVERAGE_TARGETS[*]}"
bazel coverage "${BAZEL_BUILD_OPTIONS[@]}" "${BAZEL_COVERAGE_OPTIONS[@]}" "${COVERAGE_TARGETS[@]}"

echo "Collecting profile and testlogs"
if [[ -n "${ENVOY_BUILD_PROFILE}" ]]; then
    cp -f "$(bazel info output_base)/command.profile.gz" "${ENVOY_BUILD_PROFILE}/coverage.profile.gz" || true
fi

if [[ -n "${ENVOY_BUILD_DIR}" ]]; then
    find bazel-testlogs/ -name test.log \
        | tar cf - -T - \
        | bazel run "${BAZEL_BUILD_OPTIONS[@]}" //tools/zstd -- \
                - -T0 --fast -o "${ENVOY_BUILD_DIR}/testlogs.tar.zst"
    echo "Profile/testlogs collected: ${ENVOY_BUILD_DIR}/testlogs.tar.zst"
fi

COVERAGE_DIR="${SRCDIR}/generated/coverage"
if [[ ${FUZZ_COVERAGE} == "true" ]]; then
    COVERAGE_DIR="${SRCDIR}"/generated/fuzz_coverage
fi

rm -rf "${COVERAGE_DIR}"
mkdir -p "${COVERAGE_DIR}"

if [[ ! -e bazel-out/_coverage/_coverage_report.dat ]]; then
    echo "No coverage report found (bazel-out/_coverage/_coverage_report.dat)" >&2
    exit 1
elif [[ ! -s bazel-out/_coverage/_coverage_report.dat ]]; then
    echo "Coverage report is empty (bazel-out/_coverage/_coverage_report.dat)" >&2
    exit 1
else
    COVERAGE_DATA="${COVERAGE_DIR}/coverage.dat"
    cp bazel-out/_coverage/_coverage_report.dat "${COVERAGE_DATA}"
fi

COVERAGE_VALUE="$(genhtml --prefix "${PWD}" --output "${COVERAGE_DIR}" "${COVERAGE_DATA}" | tee /dev/stderr | grep lines... | cut -d ' ' -f 4)"
COVERAGE_VALUE=${COVERAGE_VALUE%?}

echo "Compressing coveraged data"
if [[ "${FUZZ_COVERAGE}" == "true" ]]; then
    if [[ -n "${ENVOY_FUZZ_COVERAGE_ARTIFACT}" ]]; then
        tar cf - -C "${COVERAGE_DIR}" --transform 's/^\./fuzz_coverage/' . \
            | bazel run "${BAZEL_BUILD_OPTIONS[@]}" //tools/zstd -- \
                    - -T0 --fast -o "${ENVOY_FUZZ_COVERAGE_ARTIFACT}"
    fi
elif [[ -n "${ENVOY_COVERAGE_ARTIFACT}" ]]; then
     tar cf - -C "${COVERAGE_DIR}" --transform 's/^\./coverage/' . \
         | bazel run "${BAZEL_BUILD_OPTIONS[@]}" //tools/zstd -- \
                 - -T0 --fast -o "${ENVOY_COVERAGE_ARTIFACT}"
fi

if [[ "$VALIDATE_COVERAGE" == "true" ]]; then
  if [[ "${FUZZ_COVERAGE}" == "true" ]]; then
    COVERAGE_THRESHOLD=23.75
  fi
  COVERAGE_FAILED=$(echo "${COVERAGE_VALUE}<${COVERAGE_THRESHOLD}" | bc)
  if [[ "${COVERAGE_FAILED}" -eq 1 ]]; then
      echo "##vso[task.setvariable variable=COVERAGE_FAILED]${COVERAGE_FAILED}"
      echo "Code coverage ${COVERAGE_VALUE} is lower than limit of ${COVERAGE_THRESHOLD}"
      exit 1
  else
      echo "Code coverage ${COVERAGE_VALUE} is good and higher than limit of ${COVERAGE_THRESHOLD}"
  fi
fi

# We want to allow per_file_coverage to fail without exiting this script.
set +e
if [[ "$VALIDATE_COVERAGE" == "true" ]] && [[ "${FUZZ_COVERAGE}" == "false" ]]; then
  echo "Checking per-extension coverage"
  output=$(./test/per_file_coverage.sh)
  response=$?

  if [ $response -ne 0 ]; then
    echo Per-extension coverage failed:
    echo "$output"
    COVERAGE_FAILED=1
    echo "##vso[task.setvariable variable=COVERAGE_FAILED]${COVERAGE_FAILED}"
    exit 1
  fi
  echo Per-extension coverage passed.
fi

echo "HTML coverage report is in ${COVERAGE_DIR}/index.html"
