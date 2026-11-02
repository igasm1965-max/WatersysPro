// Dynamic app config.
//
// google-services.json is gitignored, so on EAS Build the file is provided
// through the file environment variable GOOGLE_SERVICES_JSON (sensitive,
// production environment). On the builder process.env.GOOGLE_SERVICES_JSON
// points to the uploaded file; locally we fall back to the local copy.
//
// NOTE: the "@GOOGLE_SERVICES_JSON" placeholder in a static app.json is NOT
// resolved by EAS (neither sensitive nor secret visibility) — a dynamic
// app.config.js with process.env is the documented way (see
// https://docs.expo.dev/eas/environment-variables/manage.md).
const appJson = require('./app.json');

const config = {
  ...appJson.expo,
};

config.android = {
  ...config.android,
  googleServicesFile: process.env.GOOGLE_SERVICES_JSON ?? './google-services.json',
};

module.exports = config;
