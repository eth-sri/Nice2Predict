
// sticky nav
// ---------------

jQuery(document).ready(function($){

    $('#nav ul').onePageNav({
        currentClass: 'selected',
        scrollOffset: 50,
        scrollThreshold: 0.5,
        changeHash: false,
        scrollSpeed: 750,
        easing: 'swing'
    });

    $('#iframe_nav a#list').click(function(){
        $('iframe#tool_results').attr('src', race_url);
        return false;

    });

    var $input = $("input#web_address");
    var oldVal = $input.val();
    $input.focus().val('').val(oldVal);

    var race_url = "";

    $('form.searchform').submit(function(){

        var tool_url =  $(this).attr('action');
        var submitted_url = $(this).find('input#web_address').val();
        race_url = tool_url+submitted_url;

        if(submitted_url=="http://"){
            alert('Please enter a full URL');
        }else {

            //TODO verify http:// in submitted url

            $('#load_error').hide();
            $('#ajax-loader').show();
            $('iframe#tool_results').attr('src', race_url);
            $('iframe#tool_results').load(function() { $('#ajax-loader').hide(); $(this).slideDown(); });
            $('iframe#tool_results').error(function() { $('#ajax-loader').hide(); $('#load_error').slideDown(); });

        }

        return false;
    });


    $('.tabs').tabs();
});

// sticky nav
// ---------------
(function($) {
    $.fn.tabs = function() {

        var tabs = $(this);
        var tab_controls  = tabs.find('.tab_controls li');


        tab_controls.each(function(id,v){

            $(v).click((function(id) {
                return function() {
                    $('.tab_content div').hide();
                    $('.tab_content div:eq('+id+')').show();
                    tab_controls.removeClass('selected');
                    $(this).addClass('selected');
                };
            }(id)));

        });

    }
})(jQuery);