# Trusted root certificates

The firmware verifies the TLS certificate of every server it talks to against these root
certificates (DER encoded, embedded at build time by `CMakeLists.txt`). Each connection is checked against
the one root for its server only, which keeps the handshake small:

| File                          | Used for                                                  | Expires        |
| ----------------------------- | --------------------------------------------------------- | -------------- |
| `usertrust_ecc_root.der`      | `api.github.com` (USERTrust ECC Certification Authority)  | 2038-01-18     |
| `globalsign_r3_root.der`      | `crates.io` (GlobalSign Root CA - R3)                     | **2029-03-18** |
| `digicert_global_g2_root.der` | `azuresearch-usnc.nuget.org` (DigiCert Global Root G2)    | 2038-01-15     |
| `gts_root_r4.der`             | `ipwho.is` (Google Trust Services GTS Root R4)            | 2036-06-22     |
| `isrg_root_x1.der`            | `api.open-meteo.com` (ISRG Root X1, Let's Encrypt)        | 2035-06-04     |

They were taken from the Python `certifi` bundle. To refresh one, export it from any trust
store as DER (`openssl x509 -outform DER`) and replace the file. If a site moves to a different
certificate authority, the board reports `TLS connect to <host> failed` and needs the new root
added here and in `src/https.c`.

Certificate expiry dates are not checked on the board, because it has no trusted clock at boot.
The certificate chain, the signatures and the host name are all verified.
