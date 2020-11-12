/* NOTE: INDIVIDUAL/GROUP ENROLLMENT
   ENROLLMENT GROUP
   All devices in the X.509 enrollment group present X.509 certificates that have been
   signed by the same root or intermediate Certificate Authority (CA).
   Each certificate must have a unique CN (Common Name), which will be the registrationID
   (and then the deviceID on the IoT Hub) of the device. Therefore, you must set the
   SECRET_REGISTRATION_ID equal to the device leaf certificate CN.

   INDIVIDUAL ENROLLMENT
   In this case, you must use the same leaf certificate that has been uploaded on the
   DPS Individual Enrollment setup.
   Therefore, it's also possible to use the Arduino self-signed certificate, and the
   SECRET_REGISTRATION_ID corresponds to the certificate CN (which is by default the
   crypto chip serial number).
   In this case, it's possible to set a personalized DEVICE_ID into the individual
   enrollment settings on Azure portal.
*/

// Fill in  your WiFi networks SSID and password
#define SECRET_SSID      "________________"
#define SECRET_PASS      "________________"

// Fill in the hostname and the ID scope of your Azure IoT DPS
#define SECRET_BROKER   "global.azure-devices-provisioning.net"
#define SECRET_ID_SCOPE "________________"

// Fill in the device id (= registration ID)
#define SECRET_REGISTRATION_ID "________________"

// Device leaf certificate (if the self-signed certificate is
// not suitable for use). It must be signed by CA Root cert or
// intermediate cert, and made with the device private key.
static const char CLIENT_CERT[] PROGMEM = R"EOF(
-----BEGIN CERTIFICATE-----
yGB98ybg)8ygb9Fb6rd5esc64a5dv87bg9Ygn=Uh=uh09uHnbYG&rv/57Rvb7btf
________________________________________________________________
________________________________________________________________
________________________________________________________________
________________________________________________________________
________________________________________________________________
Tj3L0kEK/e+SSemhOkrlRVWYEInI/CKhM2Z0yg==
-----END CERTIFICATE-----
)EOF";
