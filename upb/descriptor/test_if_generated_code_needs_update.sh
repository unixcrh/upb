#!/bin/bash
#
# Tests whether the code generated by upbc is the same as the generated
# code we currently have checked in.  But failure in this test doesn't
# indicate a bug (probably), it just means that the new generated file
# should be copied back into the source tree.

source googletest.sh || exit 1

DIR="$TEST_SRCDIR/google3/third_party/upb/upb/descriptor"

echo "Checking third_party/upb/upb/descriptor/descriptor.upb.c..."
diff -u $DIR/descriptor.upb.c $DIR/descriptor-generated.upb.c || {
  echo FAILED: generated file has changed.
  echo To fix, run:
  echo "  cp blaze-genfiles/third_party/upb/upb/descriptor/descriptor-generated.upb.c third_party/upb/upb/descriptor/descriptor.upb.c"
  exit 1
}

echo "Checking third_party/upb/upb/descriptor/descriptor.upb.h..."
diff -u $DIR/descriptor.upb.h $DIR/descriptor-generated.upb.h || {
  echo FAILED: generated file has changed.
  echo To fix, run:
  echo "  cp blaze-genfiles/third_party/upb/upb/descriptor/descriptor-generated.upb.h third_party/upb/upb/descriptor/descriptor.upb.h"
  exit 1
}
