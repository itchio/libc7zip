
gitignored dirs:

  - asio: it's just boost-less asio v1.10.6
  - lib7zip: this one is hairier, but it basically has:
    - bin/
      - 7z.dll
    - include/
      - lib7zip.h
    - lib/
      - lib7zip.a

