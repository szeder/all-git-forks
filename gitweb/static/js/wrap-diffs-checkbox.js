/**
 * called from onLoad to uncheck that box initially
 * (just to make the javascript handling simpler)
*/
function wrapCheckboxSetup(){
       var checkBoxEl = document.getElementById('wrapDiffsCheckbox');
       if (checkBoxEl === null){
           return;
       }
       checkBoxEl.checked = false;
}
/**
 * Called when the "side by side (wrap[])" box is clicked.
 *
*/
function toggleWrapping(checkBox){

    var className = "wrapped-diff";
    var selector = ".chunk_block.chg, .chunk_block.add, .chunk_block.rem";

    // ok on IE8+ http://youmightnotneedjquery.com/
    function forEachElement(selector, fn) {
        var elements = document.querySelectorAll(selector);
        for (var i = 0; i < elements.length; i++)
            fn(elements[i], i);
    }

    forEachElement(selector, function(el, i){
        // ok on IE8+ http://youmightnotneedjquery.com/
        if (checkBox.checked === true){
            if (el.classList) 
                el.classList.add(className);
            else 
                el.className += ' ' + className;
        }else{
            if (el.classList){
                el.classList.remove(className);
            } else {
                el.className = 
                    el.className.replace(
                        new RegExp('(^|\\b)' +
                            className.split(' ').join('|') + '(\\b|$)', 'gi'
                        ),
                        ' '
                    );
            }
        }

    });
}


