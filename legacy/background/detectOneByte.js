module.exports = (str) => {
    for(let i = 0; i < str.length; i++) {
        if(str.charCodeAt(i) > 191)
            return false;
    }
    return true;
}