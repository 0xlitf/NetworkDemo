import QtQuick 2.8
import NetworkClientForQml 1.0
NetworkClientForQml {
    id: NetworkClientForQml
    function onSendSucceed( callback, received ) {
        callback( received );
    }
    function onSendFail( callback ) {
        callback();
    }
}
