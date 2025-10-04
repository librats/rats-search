// https://stackoverflow.com/questions/15270902/check-for-internet-connectivity-in-nodejs
module.exports = function checkInternet(cb) {
	require('dns').lookup('google.com',function(err) {
		if (err && (err.code == "ENOTFOUND" || err.code == "EAI_AGAIN")) {
			cb(false);
		} else {
			cb(true);
		}
	})
}