$(function() {

	init();
	$(window).resize( init );

});

function init() {
	var winHeight = $(window).height();

	$('.pane').each( function() {
		$(this).css('height', winHeight);
	});
}
