function parseQuery(str) {
    var result = {};

    str.split(/[&;]/).forEach(function (s) {
        var pair = s.split(/=/, 2);
        result[ decodeURIComponent(pair[0]) ] = decodeURIComponent(pair[1]);
    });

    return result;
}

var query = parseQuery(location.search.substring(1));
['ds', 'la'].forEach(function (key) {
    if (query[key]) {
        setCookie(key, query[key], { expires: 7 });
    }
});
