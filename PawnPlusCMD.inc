/**
  * PawnPlus powered command processor prototype
  *
  * 10/10/2019 -Graber
  *
  */

#if defined _PawnPlusCMD_included
	#endinput
#endif
#define _PawnPlusCMD_included

#include <a_samp>

#include <PawnPlus>

#include <sscanf2>

#if __Pawn >= 0x030A && __PawnBuild >= 2
	#if debug < 2
		#error Script is being compiled with a debug value lower than 2. PawnPlusCMD needs debug information to properly initialize commands with auto-parameter parsing
	#endif
#endif

#if !defined MAX_FUNC_NAME
	// YSI definition
	#if defined __COMPILER_sNAMEMAX
		#define MAX_FUNC_NAME __COMPILER_sNAMEMAX
	#else
	// Assume 31
		#define MAX_FUNC_NAME (31)
	#endif
#endif

const PPCMD_DEFAULT_FLAG = 0;

#define PPCMD_DEFAULT_FLAG%9ppcmd_REMOVE_BRACKETS<%1> %1
#define ppcmd_REMOVE_BRACKETS

#define _PP_CMD_FLAG(%1,%2) \
	forward pcf@%1(); \
	public pcf@%1() \
		SetCommandFlags(#%1, PPCMD_DEFAULT_FLAG ppcmd_REMOVE_BRACKETS%2);

#define CMD%0:%1(%2)			\
		_PP_CMD_FLAG(%1,%0) \
		forward cmd_%1(%2); \
		public cmd_%1(%2)

#define COMMAND%0:%1(%2) \
			CMD%0:%1(%2)

#define command%0(%1,%2,%3) \
			CMD%0:%1(%2, %3)

#define cmd%0(%1,%2,%3) \
			CMD%0:%1(%2, %3)

#if !defined isnull
	#define isnull(%1) \
				((!(%1[0])) || (((%1[0]) == '\1') && (!(%1[1]))))
#endif

#if !defined PPCMD_DEFAULT_USAGE_MESSAGE
	#define PPCMD_DEFAULT_USAGE_MESSAGE "Usage: /"
#endif

// SmartCMD style alts
#define ALT:%1=%2CMD:%0; \
	forward pca@%1(); \
	public pca@%1() \
		RegisterCommandAlias(#%0, #%1);

// Pawn.CMD style alias
#define alias:%0(%1); \
	forward pca@%0(); \
	public pca@%0() \
		RegisterCommandAlias(#%0, %1);

// Usage macro
#define usage:%0(%1); \
	forward pcu@%0(); \
	public pcu@%0() \
		SetCommandUsageMessage(#%0, %1);

// Some extra natives I might need that don't exist in PawnPlus yet

native pawn_call_public_s(ConstAmxString:function, const format[], AnyTag:...) = pawn_call_public;
native CallbackHandler:pawn_register_callback_s(ConstAmxString:callback, ConstAmxString:handler, handler_flags:flags=handler_default, const additional_format[]="", AnyTag:...) = pawn_register_callback;
native NativeHook:pawn_add_hook_s(ConstAmxString:function, const format[], ConstAmxString:handler, const additional_format[]="", AnyTag:...) = pawn_add_hook;
native amx_public_index_s(ConstAmxString:function) = amx_public_index;
native [2]amx_encode_public_name_s(ConstAmxString:function) = amx_encode_public_name;

native bool:map_s_add_arr(Map:map, ConstAmxString:key, const AnyTag:value[], value_size=sizeof(value), TagTag:value_tag_id=tagof(value)) = map_str_add_arr;
native bool:map_s_add(Map:map, ConstAmxString:key, AnyTag:value, TagTag:value_tag_id=tagof(value)) = map_str_add;

native Symbol:debug_symbol_s(ConstAmxString:name, code=cellmin, symbol_kind:kind=symbol_kind:-1, symbol_class:class=symbol_class:-1) = debug_symbol;


static CallbackHandler:OnPlayerCmdHook;

static Map:CommandMap;

static Pool:CommandPool;

enum _:E_COMMAND_INFO
{
	E_COMMAND_EncodedIndex[2],
	List:E_COMMAND_VariantParameterList,
	Expression:E_COMMAND_SscanfExpression,
	E_COMMAND_Flags,
	bool:E_COMMAND_Disabled,
	String:E_COMMAND_BaseCommand,
	E_COMMAND_NoArgs,
	String:E_COMMAND_UsageMessage,
}

enum E_COMMAND_RESULT
{
	E_COMMAND_RESULT_Disabled = -2,
	E_COMMAND_RESULT_ParsingFailed,
	E_COMMAND_RESULT_Unexistant,
	E_COMMAND_RESULT_Success,
}

forward _pp@on_init@PPCommands();
public _pp@on_init@PPCommands()
{
	CommandMap = map_new();

	CommandPool = pool_new();

// If anyone has some issue with anything that reorders the PFT,
// you can wait 1 tick and register the commands on the next one
#if defined PP_COMMANDS_TICK_BEFORE_HOOK
	wait_ticks(1);
#endif

	// If OnPlayerCommandText doesn't exist
	if(funcidx("OnPlayerCommandText") == -1)
	{
		// Create it and always return false
		pawn_create_callback("OnPlayerCommandText", expr_false());
		// Originally, I was planning to make the created callback an expression
		// that calls the hook, to avoid the overhead of doing a hook.
		// But expr_public seems to be broken with strings at the moment,
		// so for now this will do
	}

	// Now hook the callback itself. 
	OnPlayerCmdHook = pawn_register_callback("OnPlayerCommandText", #OnPlayerCommandText_PPHook);

	// Get the count of public functions
	new const
		public_count = amx_num_publics(),
		String:single_str_param = str_new_static("s")
	;

	// Loop to find every command
	for(new i; i < public_count; ++i)
	{
		// Get the command in a PP String to use it easily on other PP functions
		new String:public_name = amx_public_name_s(i);

		// Find all public functions starting with cmd_
		// The extracted result is the command name itself
		new List:regex_results = str_extract(public_name, "^cmd_(.*?)$");
		if(regex_results)
		{
			// Create command info array structure
			new
				command_info[E_COMMAND_INFO],
				String:command_name = command_info[E_COMMAND_BaseCommand] = str_acquire(str_set_to_lower(list_get_str_s(regex_results, 1))),
				String:command_usage = str_format(PPCMD_DEFAULT_USAGE_MESSAGE"%S", command_name);
			// Encode index and save it for faster calls in the future
			command_info[E_COMMAND_EncodedIndex] = amx_encode_public_name_s(public_name);

			// Get an iterator containing the symbols (variables) of the function
			// Navigate to the second last to ignore the playerid parameter
			new Iter:debug_iterator = iter_move_previous(
				iter_to_last(
					debug_symbol_variables(
						debug_symbol_s(public_name, .kind=symbol_kind_function, .class=symbol_class_global)
					)
				)
			);

			// Make list that will serve as a cache for parsing variables
			new List:args = list_new();

			// Call specifier that gets passed to pawn_call_native
			// If I specified "+" all alone, sscanf would complain
			// about a parameter count/specifier mismatch. Probably
			// PawnPlus was pushing some parameter out of nowhere. 
			new String:call_specifier = str_new_static("ss");

			// String with sscanf specifiers for each variable
			new String:sscanf_specifiers = str_new_static("");

			new bool:no_args = true;

			for(;iter_inside(debug_iterator);iter_move_previous(debug_iterator))
			{
				new Symbol:symbol = Symbol:iter_get(debug_iterator);

				// If it's higher than 0, it's not a parameter
				if(debug_symbol_addr(symbol) <= 0)
				{
					break;
				}

				no_args = false;

				new const symbol_kind:symbol_type = debug_symbol_kind(symbol);

				// If it's an array reference (arr[] as param)
				// It's 99% a string. Maybe it's better to check
				// for tags and throw an error if it has one.
				if(symbol_type == symbol_kind_array_reference)
				{
					// Append * to the call specifier
					str_append(call_specifier, str_new_static("*"));
					// Allocate enough for a string
					list_add(args, var_acquire(var_new_buf(256)));
					// Apend that to the sscanf parser
					str_append(sscanf_specifiers, str_new_static("s[256]"));

					// same for every other tag
				}
				else
				{
					// Parser for specific tags
					new symbol_tag = debug_symbol_tag(symbol);
					if(symbol_tag == tagof(Float:))
					{
						str_append(call_specifier, str_new_static("*"));
						list_add(args, var_acquire(var_new(0.0)));
						str_append(sscanf_specifiers, str_new_static("f"));
					}
					// TODO: Player tag, special case
					else // Nothing else, fallback to cell
					{
						str_append(call_specifier, str_new_static("*"));
						list_add(args, var_acquire(var_new(0)));
						str_append(sscanf_specifiers, str_new_static("d"));
					}
				}

				str_append_format(command_usage, " [%S]", debug_symbol_name_s(symbol));
			}


			if(no_args)
			{
				command_info[E_COMMAND_NoArgs] = true;
			}
			// If only "s", don't build expression and passthrough params[]
			else if(sscanf_specifiers != single_str_param)
			{
				// Build the expression
				// It's probably better to use expr_* calls instead of expr_parse,
				// but I couldn't figure it out and just wanted to have a working
				// prototype for now. Hopefully I'll change this in the future
				new String:expression_string = str_format(
					"pawn_call_native(char:\"sscanf\", char:\"%S\", $arg0, char:\"%S\", $args1)",
					call_specifier, sscanf_specifiers
				);

				// Parse the expression, and then acquire it
				command_info[E_COMMAND_SscanfExpression] = expr_acquire(expr_parse_s(expression_string));
				command_info[E_COMMAND_VariantParameterList] = args;
				command_info[E_COMMAND_UsageMessage] = str_acquire(command_usage);
			}

			// Add it to a pool
			new const index = pool_add_arr(CommandPool, command_info);

			// Add the index to a map indexed by command name
			map_s_add(CommandMap, command_name, index);

			// Delete the regex match list
			list_delete(regex_results);
		}
	}

	for(new i; i < public_count; ++i)
	{
		new String:public_name = amx_public_name_s(i);

		if(str_extract(public_name, "^pcf@.*?$"))
		{
			pawn_call_public(amx_encode_public(i), "");
		}
	}

	for(new i; i < public_count; ++i)
	{
		new String:public_name = amx_public_name_s(i);

		// Public hook
		if(str_match(public_name, "^pca@.*?$"))
		{
			pawn_call_public(amx_encode_public(i), "");
		}
		else if(str_match(public_name, "^pcu@.*?$"))
		{
			pawn_call_public(amx_encode_public(i), "");
		}
	}
}

forward _pp@on_exit@PPCommands();
public _pp@on_exit@PPCommands()
{
	map_delete(CommandMap);

	pool_delete(CommandPool);

	if(OnPlayerCmdHook)
	{
		pawn_unregister_callback(OnPlayerCmdHook);
	}
}

forward OnPlayerCommandText_PPHook(playerid, cmdtext[]);
public OnPlayerCommandText_PPHook(playerid, cmdtext[])
{
	new pos;

	#if defined PPCMD_NO_PREFIX
	while(cmdtext[pos] > ' ')
	{
		cmdtext[pos] = tolower(cmdtext[pos++]);
	}
	#else
	while (cmdtext[++pos] > ' ')
	{
		cmdtext[pos-1] = tolower(cmdtext[pos]);
	}
	cmdtext[pos-1] = 0;
	#endif
	while (cmdtext[pos] == ' ')
	{
		cmdtext[pos++] = 0;
		//cmdtext[pos] = pos++;
	}

	//strcat(params, cmdtext[pos]);

	static const null_string[] = "\1";

	if(map_has_str_key(CommandMap, cmdtext))
	{
		new command_info[E_COMMAND_INFO];
		new const index = map_str_get(CommandMap, cmdtext);

		pool_get_arr(CommandPool, index, command_info);

		new const flags = command_info[E_COMMAND_Flags];

		if(command_info[E_COMMAND_Disabled])
		{
			if(cmdtext[pos] == 0 ? CallLocalFunction("OnPlayerCommandReceived", "dssd", playerid, cmdtext, null_string, 0) : CallLocalFunction("OnPlayerCommandReceived", "dssd", playerid, cmdtext, cmdtext[pos], 0))
			{
				return cmdtext[pos] == 0 ?
					(CallLocalFunction("OnPlayerCommandPerformed", "dssddd", playerid, cmdtext, null_string, 0, _:E_COMMAND_RESULT_Disabled, 0)) :
					(CallLocalFunction("OnPlayerCommandPerformed", "dssddd", playerid, cmdtext, cmdtext[pos], 0, _:E_COMMAND_RESULT_Disabled, 0));
			}
			return 1;
		}

		if(
			cmdtext[pos] == 0 ?
			CallLocalFunction("OnPlayerCommandReceived", "dssd", playerid, cmdtext, null_string, flags) :
			CallLocalFunction("OnPlayerCommandReceived", "dssd", playerid, cmdtext, cmdtext[pos], flags)
		)
		{
			new Expression:original_expression = command_info[E_COMMAND_SscanfExpression];

			if(command_info[E_COMMAND_NoArgs])
			{
				return cmdtext[pos] == 0 ?
				(CallLocalFunction(
					"OnPlayerCommandPerformed", "dssddd", playerid, cmdtext, null_string,
					flags, _:E_COMMAND_RESULT_Success, pawn_call_public(command_info[E_COMMAND_EncodedIndex], "d", playerid)
				)) :
				(CallLocalFunction(
					"OnPlayerCommandPerformed", "dssddd", playerid, cmdtext, cmdtext[pos],
					flags, _:E_COMMAND_RESULT_Success, pawn_call_public(command_info[E_COMMAND_EncodedIndex], "d", playerid)
				));
			}
			else if(original_expression)
			{
				new List:buffer_list = command_info[E_COMMAND_VariantParameterList];

				new Expression:current_expression = expr_bind(original_expression, expr_const_str(cmdtext[pos]));

				for(new Iter:arg=list_iter(buffer_list); iter_inside(arg); iter_move_next(arg))
				{
					current_expression = expr_bind(current_expression, expr_const(_:var_addr_const(Variant:iter_get(arg))));
				}

				new const sscanf_result = expr_get(current_expression);

				if(sscanf_result == 0)
				{
					new List:arguments_list = list_new();

					for(new Iter:arg=list_iter(buffer_list);iter_inside(arg);iter_move_next(arg))
					{
						list_add_var(arguments_list, Variant:iter_get(arg));
					}

					new const return_value = cmdtext[pos] == 0 ?
					(CallLocalFunction(
						"OnPlayerCommandPerformed", "dssddd", playerid, cmdtext, null_string,
						flags, _:E_COMMAND_RESULT_Success, pawn_call_public(command_info[E_COMMAND_EncodedIndex], "dl", playerid, arguments_list)
					)) :
					(CallLocalFunction(
						"OnPlayerCommandPerformed", "dssddd", playerid, cmdtext, cmdtext[pos],
						flags, _:E_COMMAND_RESULT_Success, pawn_call_public(command_info[E_COMMAND_EncodedIndex], "dl", playerid, arguments_list)
					));

					list_delete(arguments_list);

					return return_value;
				}
				else
				{
					if(cmdtext[pos] == 0)
					{
						pawn_call_public(
							"OnPlayerCommandParsingFailed", "dssdS", playerid, cmdtext, null_string,
							flags, command_info[E_COMMAND_UsageMessage]
						);
					}
					else
					{
						pawn_call_public(
							"OnPlayerCommandParsingFailed", "dssdS", playerid, cmdtext, cmdtext[pos],
							flags, command_info[E_COMMAND_UsageMessage]
						);
					}

					return cmdtext[pos] == 0 ? (CallLocalFunction(
						"OnPlayerCommandPerformed", "dssddd", playerid, cmdtext, null_string,
						flags, _:E_COMMAND_RESULT_ParsingFailed, 0
					)) :
					(CallLocalFunction(
						"OnPlayerCommandPerformed", "dssddd", playerid, cmdtext, cmdtext[pos],
						flags, _:E_COMMAND_RESULT_ParsingFailed, 0
					));
				}
			}
			else
			{
				new const return_value = cmdtext[pos] == 0 ?
				(CallLocalFunction(
					"OnPlayerCommandPerformed", "dssddd", playerid, cmdtext, null_string,
					flags, _:E_COMMAND_RESULT_Success, pawn_call_public(command_info[E_COMMAND_EncodedIndex], "ds", playerid, null_string)
				)) :
				(CallLocalFunction(
					"OnPlayerCommandPerformed", "dssddd", playerid, cmdtext, cmdtext[pos],
					flags, _:E_COMMAND_RESULT_Success, pawn_call_public(command_info[E_COMMAND_EncodedIndex], "ds", playerid, cmdtext[pos])
				));

				return return_value;
			}
		}
		return 0;
	}
	else
	{
		if(cmdtext[pos] == 0 ?
			CallLocalFunction("OnPlayerCommandReceived", "dssd", playerid, cmdtext, null_string, 0) :
			CallLocalFunction("OnPlayerCommandReceived", "dssd", playerid, cmdtext, cmdtext[pos], 0)
		)
		{
			return cmdtext[pos] == 0 ?
			(CallLocalFunction("OnPlayerCommandPerformed", "dssddd", playerid, cmdtext, null_string, 0, _:E_COMMAND_RESULT_Unexistant, 0)) :
			(CallLocalFunction("OnPlayerCommandPerformed", "dssddd", playerid, cmdtext, cmdtext[pos], 0, _:E_COMMAND_RESULT_Unexistant, 0));
		}
		return 1;
	}
}

forward OnPlayerCommandReceived(playerid, cmd[], params[], flags);
forward OnPlayerCommandPerformed(playerid, cmd[], params[], flags, E_COMMAND_RESULT:result, return_value);
forward OnPlayerCommandParsingFailed(playerid, cmd[], params[], flags, usage[]);

stock RegisterCommandAlias(const command[], ...)
{
	static const args_format[][] =
	{
		"", "s", "ss", "sss", "ssss", "sssss", "ssssss", "sssssss", "ssssssss", "sssssssss",
		"ssssssssss", "sssssssssss", "ssssssssssss", "sssssssssssss", "ssssssssssssss",
		"sssssssssssssss", "ssssssssssssssss", "sssssssssssssssss", "ssssssssssssssssss",
		"sssssssssssssssssss", "ssssssssssssssssssss", "sssssssssssssssssssss",
		"ssssssssssssssssssssss", "sssssssssssssssssssssss", "ssssssssssssssssssssssss",
		"sssssssssssssssssssssssss", "ssssssssssssssssssssssssss", "sssssssssssssssssssssssssss",
		"ssssssssssssssssssssssssssss", "sssssssssssssssssssssssssssss", "ssssssssssssssssssssssssssssss",
		"sssssssssssssssssssssssssssssss", "ssssssssssssssssssssssssssssssss"
	};

	new const argument_count = numargs();

	if(argument_count <= 1)
	{
		printf("[PPCMD] Incorrect parameter count for RegisterCommandAlias (more than 1 expected - got %d)", argument_count);
		return;
	}

	if(!map_has_str_key(CommandMap, command))
	{
		printf("[PPCMD] RegisterCommandAlias: Command \"/%s\" doesn't exist", command);
		return;
	}

	new const index = map_str_get(CommandMap, command);

	new const List:arguments_list = pawn_get_args(args_format[argument_count], true);

	for(new Iter:arg=iter_move_next(list_iter(arguments_list));iter_inside(arg);iter_move_next(arg))
	{
		new string[MAX_FUNC_NAME + 1];
		iter_get_str(arg, string);

		map_str_add(CommandMap, string, index);
	}

	list_delete(arguments_list);
}

stock SetCommandFlags(const command[], flags)
{
	if(!map_has_str_key(CommandMap, command))
	{
		printf("[PPCMD] SetCommandFlags: Command \"/%s\" doesn't exist", command);
		return;
	}

	new command_info[E_COMMAND_INFO];
	new const index = map_str_get(CommandMap, command);

	pool_get_arr(CommandPool, index, command_info);
	command_info[E_COMMAND_Flags] = flags;

	pool_set_arr(CommandPool, index, command_info);
}

stock GetCommandBaseName(const command[], base_command[], size = sizeof base_command)
{
	if(!map_has_str_key(CommandMap, command))
	{
		printf("[PPCMD] GetCommandBaseName: Command \"/%s\" doesn't exist", command);
		return 0;
	}

	new command_info[E_COMMAND_INFO];
	new const index = map_str_get(CommandMap, command);

	pool_get_arr(CommandPool, index, command_info);
	str_get(command_info[E_COMMAND_BaseCommand], base_command, size);

	return 1;
}

stock String:GetCommandBaseNameStr(const command[])
{
	if(!map_has_str_key(CommandMap, command))
	{
		printf("[PPCMD] GetCommandBaseNameStr: Command \"/%s\" doesn't exist", command);
		return INVALID_STRING;
	}

	new command_info[E_COMMAND_INFO];
	new const index = map_str_get(CommandMap, command);

	return String:pool_get(CommandPool, index, _:E_COMMAND_BaseCommand);
}

stock SetCommandUsageMessage(const command[], const message[], message_size = sizeof message)
{
	if(!map_has_str_key(CommandMap, command))
	{
		printf("[PPCMD] GetCommandBaseNameStr: Command \"/%s\" doesn't exist", command);
		return 0;
	}

	new const index = map_str_get(CommandMap, command);
	new const String:old_message = String:pool_get(CommandPool, index, _:E_COMMAND_UsageMessage);

	if(old_message)
	{
		str_release(old_message);
	}

	pool_set_cell(CommandPool, index, _:E_COMMAND_UsageMessage, str_acquire(str_new_static(message, .size = message_size)));
	return 1;
}

stock GetCommandPool()
{
	return CommandPool;
}
