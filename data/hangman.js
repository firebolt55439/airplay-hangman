function isMobile(){
	return (/Android|webOS|iPhone|iPad|iPod|BlackBerry|IEMobile|Opera Mini/i.test(navigator.userAgent));
}

function displayAlert(title, body, alert_context_class, fade_timeout){
	fade_timeout = fade_timeout || 2000;
	var width = "33", left = "33";
	if(isMobile()){
		left = "15";
		width = "70";
	}
	var alert = $('<div class="alert text-center alert-dismissible disp-alert" style="z-index:1000; position:fixed; width: ' + width + '%; left: ' + left + '%;"> \
		<button type="button" class="close" data-dismiss="alert" aria-label="Close"><span aria-hidden="true">&times;</span></button> \
		<strong class="guess-strong">' + title + '</strong><p class="guess-p">' + body + '</p> \
	</div>');
	if(alert_context_class) alert.addClass(alert_context_class);
	$('.disp-alert').each(function(){ $(this).hide(); });
	alert.prependTo("body");
	alert.delay(fade_timeout).fadeOut("slow", function(){ $(this).remove(); });
}

function reloadWord(){
	// Get the word.
	var success = false;
	$.ajax({
		type: "GET",
		url: "/getBlankedWord",
		async: false
	}).done(function(data){
		success = true;
		$('#blanked').text(data["blanked"]);
		$('.blanked-word-header').text("Word (" + data["length"] + " letters)");
	});
	return success;
}

var lastTableInd = -1;
var lastChild;
var currentIP; // used for IP-specific messages

function reloadTable(){
	// Get the current game state.
	var success = false;
	$.ajax({
		type: "GET",
		url: "/getGameInfo",
		async: false
	}).done(function(data){
		console.log(data);
		success = true;
		if("index" in data){
			if((data["index"] > lastTableInd || data["index"] == 0 || data["result"] != -1)){
				currentIP = data["ip_addr"];
				if(data["index"] == 0){
					// If disconnected and later reconnected (or if the server restarts), empty the history table.
					$('.history-table-body').empty();
				}
				if(data["result"] != -1 && data["index"] == lastTableInd){
					if(data["level"] != 1e9){
						data["level"] = lastChild.children().eq(0); // since the new level will be broadcasted, not the old one
					}
					lastChild.remove();
				}
				lastTableInd = data["index"];
				// <th>Level</th><th>Word</th><th>Result</th><th>Score</th>
				var result = data["result"];
				var context = (result == 0 ? "danger" : "success");
				if(result == -1) context = "active";
				var result_txt = (result == 0 ? "Lost" : "Won");
				if(result == -1) result_txt = "(ongoing)";
				var word = (data["word"].length > 0 ? data["word"] : "TBD");
				if(data["level"] == 1e9){
					data["level"] = "N/A";
				}
				if(data["score"] == -1e9){
					data["score"] = "N/A";
				}
				lastChild = $('<tr class="' + context + '"><td>' + data["level"] + '</td><td>' + word + '</td><td>' + result_txt + '</td><td>' + data["score"] + '</td></tr>');
				lastChild.appendTo($('.history-table-body'));
			}
			if(data["waitingForWord"] == true){
				$('#promptModal').modal("show");
			} else {
				$('#promptModal').modal("hide");
			}
		}
	});
	return success;
}

var lastAlert;

function reloadAlerts(){
	// Check for an alert.
	var success = false;
	$.ajax({
		type: "GET",
		url: "/getLatestAlert",
		async: false
	}).done(function(data){
		success = true;
		var alert = data["alert"];
		var showing = false;
		if(alert.length > 0){
			if(alert == lastAlert){
				return;
			}
			lastAlert = alert;
			var res = alert.match(/[%][a-zA-Z0-9]+[(][\/a-zA-Z0-9'\.:_\?, ]+[)]/g);
			console.log(res);
			if(res){
				for(var i = 0; i < res.length; i++){
					var name = res[i].match(/[%][a-zA-Z0-9]+/g)[0].substr(1);
					var args = res[i].match(/[(][\/a-zA-Z0-9'\.:_\?, ]+[)]/g)[0].substr(1).slice(0, -1).split(',');
					args = args.map(function(x){
						return x.trim();
					});
					console.log(name);
					console.log(args);
					if(name == "prompt"){
						showing = true;
						// format: %prompt(Title, /url, variable_name_in_url, Label, Type ("text"|"number"|"choice"))
						$('#promptGenWord').hide();
						$('#promptGenTitle').text(args[0]);
						$('#promptGenWordLbl').text(args[3]);
						$('#promptGeneralChoice').hide();
						$('#promptGeneralWordFill').hide();
						$('#promptGenBtn').show();
						$('#promptGenBtn').unbind("click");
						if(args[4] != "choice" && args[4] != "wordFill"){
							$('#promptGenWord').show();
							$('#promptGenWord').attr('type', args[4]);
							$('#promptGenBtn').click(function() {
								return function(args) {
									var ret = false;
									var data_map = {};
									data_map[args[2]] = $('#promptGenWord').val();
									$.ajax({
										type: "GET",
										url: args[1],
										data: data_map,
										async: false
									}).done(function(data){
										if(!$('#promptGenErrIcon').hasClass('hidden')){
											$('#promptGenErrIcon').addClass('hidden');
										}
										$('#promptGenForm').removeClass('has-feedback').removeClass('has-error');
										$('#promptGenHelp').text('');
										if(!data["success"]){
											$('#promptGenForm').addClass('has-error').addClass('has-feedback');
											$('#promptGenErrIcon').removeClass('hidden');
											$('#promptGenHelp').text(data["error"]);
										} else {
											ret = true;
											$('#promptGenWord').val('');
										}
										reloadInterface();
									});
									$('#promptGenBtn').blur();
									return ret;
								}(args);
							});
						} else if(args[4] == "choice"){
							$('#promptGenBtn').hide();
							$('#promptGeneralChoice').show();
							$('.prompt-btn-choice').each(function(args){
								return function() {
									var data_map = {};
									data_map[args[2]] = $(this).val().toLowerCase();
									$(this).unbind("click");
									$(this).click(function() {
										$.ajax({
											type: "GET",
											url: args[1],
											data: data_map,
											async: false
										}).done(function(data){
											$('#promptGenHelp').text('');
											if(!data["success"]){
												$('#promptGenHelp').text(data["error"]);
											} else {
												$('#promptGenWord').val('');
											}
											reloadInterface();
										});
										$('.prompt-btn-choice').each(function(){
											$(this).blur();
										});
										return true;
									});
								};
							}(args));
						} else if(args[4] == "wordFill"){
							$('#promptGeneralWordFill').show();

							// Fill in the word form.
							$.ajax({
								type: "GET",
								url: "/getWordFillForm",
								async: false
							}).done(function(data){
								$('#promptGeneralWordFill').html(data);
							});

							// Install click handler.
							$('#promptGenBtn').click(function() {
								return function(args) {
									var ret = false;
									var data_map = {};
									var dataVal = "";
									$('#promptGeneralWordFill').children().first().children().each(function() {
										var val = $(this).val();
										if($(this).val().length == 0) val = $(this).html();
										console.log(val);
										dataVal += val;
									});
									data_map[args[2]] = dataVal;
									$.ajax({
										type: "GET",
										url: args[1],
										data: data_map,
										async: false
									}).done(function(data){
										$('#promptGenHelp').text('');
										if(!data["success"]){
											$('#promptGenHelp').text(data["error"]);
										} else {
											ret = true;
											$('#promptGenWord').val('');
										}
										reloadInterface();
									});
									$('#promptGenBtn').blur();
									return ret;
								}(args);
							});
						}
					}
				}
			} else {
				displayAlert("Server Broadcast", data["alert"], "alert-info", 5000);
			}
		}
		if(!showing){
			$('#promptGeneral').modal("hide");
		} else {
			$('#promptGeneral').modal("show");
		}
	});
	return success;
}

function reloadPercentage(){
	// Get the percentage.
	var success = false;
	$.ajax({
		type: "GET",
		url: "/guessPercentage",
		async: false
	}).done(function(data){
		success = true;
		$('#guess_progress .progress-bar').html(data["percentage"] + '%');
		var min_width = (isMobile() ? 10.0 : 3.5);
		var width = Math.max(data["percentage"], min_width) + '%';
		$('#guess_progress .progress-bar').css('width', width);
		if(data["percentage"] > 50 && !$('#guess_progress .progress-bar').hasClass('progress-bar-danger')){
			$('#guess_progress .progress-bar').addClass('progress-bar-danger');
		} else if(data["percentage"] <= 50){
			$('#guess_progress .progress-bar').removeClass('progress-bar-danger');
		}
	});
	return success;
}

function reloadLetters(){
	// Clear the letter selection, but save which one was selected.
	var selection = $('#letter option:selected').val();
	var node = document.getElementById('letter');
	while(node.firstChild){
		node.removeChild(node.firstChild);
	}

	// Initialize the letter selection with letters that have not already been guessed.
	var success = false;
	var letters;
	$.ajax({
		type: "GET",
		url: "/getExtantLetters",
		async: false
	}).done(function(data){
		success = true;
		letters = data["letters"];
		for(var i = 0; i < letters.length; i++){
			$('#letter').append('<option value="' + letters[i] + '">' + letters[i] + '</option>');
		}
		$('#letter').val(selection);
		if($('#letter option:selected').val() === undefined){
			$('#letter').val(letters[0]);
		}
	});

	// Handle alphabet buttons.
	for(var i = 65; i <= 90; i++){
		var letter = String.fromCharCode(i);
		var id = '#letter' + letter.toUpperCase();
		var li = $(id);
		if(letters.indexOf(letter.toLowerCase()) == -1){
			if(!li.hasClass('inactive')) li.addClass('inactive');
		} else if(li.hasClass('inactive')){
			li.removeClass('inactive');
		}
	}
	return success;
}

function reloadInterface(){
	// Reload the interface.
	var success = reloadWord() &&
			reloadTable()      &&
		    reloadPercentage() &&
		    reloadLetters()    &&
		    reloadAlerts(); // all will have to be evaluated until the first one fails (e.g. returns undefined/false)
	if(!success){
		console.log("Disconnected!");
		$('#disconnected_panel').show();
	} else {
		$('#disconnected_panel').hide();
	}
}

function guessLetter(letter){
	$.ajax({
		type: "GET",
		url: "/guessLetter",
		data: {'letter': letter},
		async: false
	}).done(function(data){
		console.log(data);
		// Reload the letters.
		console.log(data);
		reloadInterface();

		// Generate the title.
		var title;
		if(data["error"]) title = "Error!";
		else if(data["success"]) title = "Correct!";
		else title = "Incorrect!";

		// Generate the alert.
		var context_class;
		if(data["error"] || !data["success"]) context_class = 'alert-danger';
		else context_class = 'alert-success';
		displayAlert(title, data["message"], context_class);
	});
}

$(document).ready(function() {
	// Set up the guessing panel output.
	$('#guess_panel').hide();

	// Set up the modal.
	$('#promptModal').modal("hide");
	$('#promptModal').removeClass("hidden");

	// Set up the modal form.
	$('#promptwordbtn').click(function() {
		var ret = false;
		$.ajax({
			type: "GET",
			url: "/chooseWord",
			data: {'word': $('#promptword').val()},
			async: false
		}).done(function(data){
			console.log(data);
			if(!$('#prompterricon').hasClass('hidden')){
				$('#prompterricon').addClass('hidden');
			}
			$('#promptForm').removeClass('has-feedback').removeClass('has-error');
			$('#promptHelp').text('');
			if(!data["success"]){
				$('#promptForm').addClass('has-error').addClass('has-feedback');
				$('#prompterricon').removeClass('hidden');
				$('#promptHelp').text(data["error"]);
			} else {
				ret = true;
				$('#promptword').val('');
			}
			reloadInterface();
		});
		$('#promptwordbtn').blur();
		return ret;
	});

	// Set up the alphabet.
  	var alphabet = $('#alphabet');
  	var no_touch = (!("ontouchstart" in document.documentElement) ? "no-touch" : "");
  	for(var i = 65; i <= 90; i++){
  		var letter = String.fromCharCode(i);
  		var li = $('<li id="letter' + letter + '" class="' + no_touch + '">' + letter + '</li>');
  		li.appendTo(alphabet);
  	}
  	alphabet.click(function(e) {
  		if(e.target != this){
  			// Clicked on child, check if it was an alphabet li, and if so, guess that.
  			var child = $(e.target);
  			var id = child.attr('id');
  			if(id.startsWith("letter") && id.length == 7){
  				child.blur();
  				guessLetter(id[6].toLowerCase().charCodeAt(0));
  			}
  		}
  	});
  	$(document).keypress(function(e) {
  		if($(document.body).hasClass("modal-open")){
  			return;
  		}
  		var code = (e.keyCode || e.which);
  		guessLetter(code);
	});

  	// Reload the interface at regular intervals.
	reloadInterface();
	setInterval(reloadInterface, 3000);

	// Handle guesses.
	$('#guessbtn').click(function() {
		var letter = $('#letter option:selected').val().charCodeAt(0);
		guessLetter(letter);
		$('#guessbtn').blur();
		return false;
	});
});
