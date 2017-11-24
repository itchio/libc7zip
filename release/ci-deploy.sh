#!/bin/sh

# upload all artifacts from a single worker
gsutil -m cp -r -a public-read compile-artifacts/* gs://dl.itch.ovh/libc7zip/