/*
 * Copyright (c) 2012 Jaseem Abid
 *
 * test_description='JavaScript tests in gitweb'
 *
 * This test tries to run JavaScript tests for gitweb with nodejs and mocha
 *
 * Run this script as $ mocha test/test.js with arguments specifed in
 * test/mocha.opts
 *
 * Sample : $ `mocha --require should -R tap test.js` for tap output
 * Sample : $ `mocha --require should -R spec test.js` or
 *          $ `mocha --require should -R landing test.js` for a much better
 * human readble output
 *
 */

var dateTime = require('../dateTime.js');
global.gitweb = dateTime.gitweb;
var dateTimeSpec = require('./dateTime.spec.js');
