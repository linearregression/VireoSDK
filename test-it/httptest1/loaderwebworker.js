(function () {
    'use strict';

    var domReady = function (callback) {
        if (document.readyState === 'loading') {
            document.addEventListener('DOMContentLoaded', callback);
        } else {
            callback();
        }
    };

    var main = function () {
        var viaCode = document.getElementById('viacode').textContent;
        var viaCodeHttp = document.getElementById('viacodehttp').textContent;
        var worker = new Worker('hostwebworker.js');

        worker.postMessage({
            type: 'init',
            data: [
                '../../dist/vireo.js',
                '../../source/core/module_coreHelpers.js',
                '../../source/io/module_eggShell.js',
                '../../source/io/module_httpClient.js',
                '../../source/core/vireo.loader.js'
            ]
        });

        worker.postMessage({
            type: 'loadAndRunSync',
            data: viaCode
        });

        worker.postMessage({
            type: 'loadAndRunSync',
            data: viaCodeHttp
        });
    };

    domReady(main);
}());
