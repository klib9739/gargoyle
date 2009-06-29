#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>

#include <erics_tools.h>
#include <uci.h>
#include <ipt_bwctl.h>

void restore_backup_for_id(char* id, char* quota_backup_dir);


void delete_chain_from_table(char* table, char* delete_chain);
void run_shell_command(char* command, int free_command_str);
char** get_shell_command_output_lines(char* command, unsigned long* num_lines, int free_command);
void free_split_pieces(char** split_pieces);
void free_null_terminated_string_array(char** strings);

list* get_all_sections_of_type(struct uci_context *ctx, char* package, char* section_type);
char* get_uci_option(struct uci_context* ctx,char* package_name, char* section_name, char* option_name);
char* get_option_value_string(struct uci_option* uopt);

int main(int argc, char** argv)
{

	char* wan_if = NULL;
	char* local_subnet = NULL;
	char* death_mark = NULL;
	char* death_mask = NULL;
	char* backup_crontab_line = NULL;
	
	char c;
	while((c = getopt(argc, argv, "W:w:l:L:d:D:m:M:c:C:")) != -1) //section, page, css includes, javascript includes, title, output interface variables
	{
		switch(c)
		{
			case 'W':
			case 'w':
				wan_if = strdup(optarg);
				break;
			case 'L':
			case 'l':
				local_subnet = strdup(optarg);
				break;
			case 'D':
			case 'd':
				death_mark = strdup(optarg);
				break;
			case 'M':
			case 'm':
				death_mask = strdup(optarg);
				break;
			case 'C':
			case 'c':
				backup_crontab_line = strdup(optarg);
				break;

		}
	}

	/* even if parameters are wrong, whack old rules */
	char quota_table[] = "mangle";
	delete_chain_from_table(quota_table, "egress_quotas");
	delete_chain_from_table(quota_table, "ingress_quotas");
	delete_chain_from_table(quota_table, "combined_quotas");
	delete_chain_from_table(quota_table, "forward_quotas");


	if(wan_if == NULL)
	{
		fprintf(stderr, "ERRROR: No wan interface specified\n");
		return 0;
	}
	if(local_subnet == NULL)
	{
		fprintf(stderr, "ERRROR: No local subnet specified\n");
		return 0;
	}
	if(death_mark == NULL)
	{
		fprintf(stderr, "ERRROR: No death mark specified\n");
		return 0;
	}
	if(death_mask == NULL)
	{
		fprintf(stderr, "ERRROR: No death mask specified\n");
		return 0;
	}



	struct uci_context *ctx = uci_alloc_context();

	list* quota_sections = get_all_sections_of_type(ctx, "firewall", "quota");
	if(quota_sections->length > 0)
	{
		run_shell_command(dynamic_strcat(3, "iptables -t ", quota_table, " -N forward_quotas 2>/dev/null"), 1);
		run_shell_command(dynamic_strcat(3, "iptables -t ", quota_table, " -N egress_quotas 2>/dev/null"), 1);
		run_shell_command(dynamic_strcat(3, "iptables -t ", quota_table, " -N ingress_quotas 2>/dev/null"), 1);
		run_shell_command(dynamic_strcat(3, "iptables -t ", quota_table, " -N combined_quotas 2>/dev/null"), 1);

		char* no_death_mark_test = dynamic_strcat(3, " -m connmark --mark 0x0/", death_mask, " ");
		run_shell_command(dynamic_strcat(6, "iptables -t ", quota_table, " -I INPUT  1 -i ", wan_if, no_death_mark_test, " -j ingress_quotas  2>/dev/null"), 1);
		run_shell_command(dynamic_strcat(6, "iptables -t ", quota_table, " -I INPUT  2 -i ", wan_if, no_death_mark_test, " -j combined_quotas 2>/dev/null"), 1);
		run_shell_command(dynamic_strcat(6, "iptables -t ", quota_table, " -I OUTPUT 1 -o ", wan_if, no_death_mark_test, " -j egress_quotas   2>/dev/null"), 1);
		run_shell_command(dynamic_strcat(6, "iptables -t ", quota_table, " -I OUTPUT 2 -o ", wan_if, no_death_mark_test, " -j combined_quotas 2>/dev/null"), 1);
		
		run_shell_command(dynamic_strcat(3, "iptables -t ", quota_table, " -I FORWARD -j forward_quotas 2>/dev/null"), 1);
		run_shell_command(dynamic_strcat(6, "iptables -t ", quota_table, " -A forward_quotas -o ", wan_if, no_death_mark_test, " -j egress_quotas  2>/dev/null"), 1);
		run_shell_command(dynamic_strcat(6, "iptables -t ", quota_table, " -A forward_quotas -i ", wan_if, no_death_mark_test, " -j ingress_quotas  2>/dev/null"), 1);
		run_shell_command(dynamic_strcat(6, "iptables -t ", quota_table, " -A forward_quotas -i ", wan_if, no_death_mark_test, " -j CONNMARK --set-mark 0x0F000000/0x0F000000  2>/dev/null"), 1);
		run_shell_command(dynamic_strcat(6, "iptables -t ", quota_table, " -A forward_quotas -o ", wan_if, no_death_mark_test, " -j CONNMARK --set-mark 0x0F000000/0x0F000000  2>/dev/null"), 1);
		run_shell_command(dynamic_strcat(3, "iptables -t ", quota_table, " -A forward_quotas -m connmark --mark 0x0F000000/0x0F000000 -j combined_quotas  2>/dev/null"), 1);
		run_shell_command(dynamic_strcat(3, "iptables -t ", quota_table, " -A forward_quotas -j CONNMARK --set-mark 0x0/0x0F000000  2>/dev/null"), 1);
		free(no_death_mark_test);

		run_shell_command(dynamic_strcat(5, "iptables -t ", quota_table, " -A egress_quotas   -j CONNMARK --set-mark 0x0/", death_mask, "  2>/dev/null"), 1);
		run_shell_command(dynamic_strcat(5, "iptables -t ", quota_table, " -A ingress_quotas  -j CONNMARK --set-mark 0x0/", death_mask, "  2>/dev/null"), 1);
		run_shell_command(dynamic_strcat(5, "iptables -t ", quota_table, " -A combined_quotas -j CONNMARK --set-mark 0x0/", death_mask, "  2>/dev/null"), 1);
		run_shell_command(dynamic_strcat(5, "iptables -t ", quota_table, " -A egress_quotas   -j CONNMARK --set-mark 0x0/0xFF000000 2>/dev/null"), 1);
		run_shell_command(dynamic_strcat(5, "iptables -t ", quota_table, " -A ingress_quotas  -j CONNMARK --set-mark 0x0/0xFF000000 2>/dev/null"), 1);
		run_shell_command(dynamic_strcat(5, "iptables -t ", quota_table, " -A combined_quotas -j CONNMARK --set-mark 0x0/0xFF000000 2>/dev/null"), 1);

		char* set_death_mark = dynamic_strcat(5, " -j CONNMARK --set-mark ", death_mark, "/", death_mask, " ");
		char* other_quota_section_name = NULL;
		unlock_bandwidth_semaphore_on_exit();
		while(quota_sections->length > 0 && other_quota_section_name == NULL)
		{

			char* next_quota = NULL;
			int process_other_quota = 0;
			if(quota_sections->length > 0)
			{
				next_quota = shift_list(quota_sections);
			}
			else
			{
				process_other_quota = 1;
				next_quota = other_quota_section_name;
				other_quota_section_name = NULL;
			}
			
			char* quota_enabled_var  = get_uci_option(ctx, "firewall", next_quota, "quota_enabled");
			int enabled = 1;
			if(quota_enabled_var != NULL)
			{
				if(strcmp(quota_enabled_var, "0") == 0)
				{
					enabled = 0;
				}
			}
			free(quota_enabled_var);
			
			if(enabled)
			{
				char* ip = get_uci_option(ctx, "firewall", next_quota, "ip");
				if(ip == NULL) { ip = strdup("ALL"); }
				if( (strcmp(ip, "ALL_OTHERS_COMBINED") == 0 || strcmp(ip, "ALL_OTHERS_INDIVIDUAL")) && (!process_other_quota)  )
				{
					other_quota_section_name = strdup(next_quota);
				}
				else
				{
					char* ignore_backup  = get_uci_option(ctx, "firewall", next_quota, "ignore_backup_at_next_restore");
					int do_restore = 1;
					if(ignore_backup != NULL)
					{
						do_restore = strcmp(ignore_backup, "0") == 0 ? 0 : 1;
						if(!do_restore)
						{
							//remove variable from uci & commit
						}
					}
					free(ignore_backup);


					char* reset_interval = get_uci_option(ctx, "firewall", next_quota, "reset_interval");
					char* reset = strdup("");
					if(reset_interval != NULL)
					{
						char* reset_time     = get_uci_option(ctx, "firewall", next_quota, "reset_time");
						char* interval_option = strdup(" --reset_interval ");
						reset = dcat_and_free(&reset, &interval_option, 1, 1);
						reset = dcat_and_free(&reset, &reset_interval, 1, 1);
						if(reset_time != NULL)
						{
							char* reset_option = strdup(" --reset_time ");
							reset = dcat_and_free(&reset, &reset_option, 1, 1);
							reset = dcat_and_free(&reset, &reset_time, 1, 1);
						}
					}
					char* types[] = { "ingress_limit", "egress_limit", "combined_limit" };
					char* postfixes[] = { "_ingress", "_egress", "_combined" };
					char* chains[] =  { "ingress_quotas", "egress_quotas", "combined_quotas" };
					char* applies_to = strdup("combined");
					char* subnet_definition = strdup("");
					int type_index;
					for(type_index=0; type_index < 3; type_index++)
					{
						char* limit = get_uci_option(ctx, "firewall", next_quota, types[type_index]);
						if(limit != NULL)
						{
							char* type_id = dynamic_replace(ip, "/", "_");
							type_id = dcat_and_free(&type_id, &(postfixes[type_index]), 1, 0);	
							
							char* ip_test = strdup(""); 
							if( strcmp(ip, "ALL_OTHERS_COMBINED") != 0 && strcmp(ip, "ALL_OTHERS_INDIVIDUAL") != 0 && strcmp(ip, "ALL") != 0 )
							{
								char* dst_test = dynamic_strcat(3, " --dst ", ip, " "); 
								char* src_test = dynamic_strcat(3, " --src ", ip, " ");
								
								run_shell_command(dynamic_strcat(5, "iptables -t ", quota_table, " -A ", chains[type_index], " -j CONNMARK --set-mark 0xF0000000/0xF0000000 2>/dev/null"), 1);
								if(strcmp(types[type_index], "egress_limit") == 0)
								{
									ip_test=dcat_and_free(&ip_test, &src_test, 1, 0);
								}
								else if(strcmp(types[type_index], "ingress_limit") == 0)
								{
									ip_test=dcat_and_free(&ip_test, &dst_test, 1, 0);
								}
								else if(strcmp(types[type_index], "combined_limit") == 0)
								{
									run_shell_command(dynamic_strcat(8, "iptables -t ", quota_table, " -A ", chains[type_index], " -i ", wan_if, dst_test, " -j CONNMARK --set-mark 0xFF000000/0xFF000000 2>/dev/null"), 1);
									run_shell_command(dynamic_strcat(8, "iptables -t ", quota_table, " -A ", chains[type_index], " -o ", wan_if, src_test, " -j CONNMARK --set-mark 0xFF000000/0xFF000000 2>/dev/null"), 1);
									char* rule_end = strdup(" -m connmark --mark 0x0F000000/0x0F000000 ");
									ip_test = dcat_and_free(&ip_test, &rule_end, 1, 1);
								}
								free(dst_test);
								free(src_test);
							}
							else if( strcmp(ip, "ALL_OTHERS_COMBINED") == 0 || strcmp(ip, "ALL_OTHERS_INDIVIDUAL") == 0 )
							{
								char* rule_end = strdup(" -m connmark --mark 0x0/0xF0000000 ");
								ip_test=dcat_and_free(&ip_test, &rule_end, 1, 1);
								if(strcmp(ip, "ALL_OTHERS_INDIVIDUAL"))
								{
									free(applies_to);
									if(strcmp(types[type_index], "egress_limit") == 0)
									{
										applies_to = strdup("individual_src");
									}
									else if(strcmp(types[type_index], "ingress_limit") == 0)
									{
										applies_to = strdup("individual_dst");
									}
									else if(strcmp(types[type_index], "combined_limit") == 0)
									{
										applies_to = strdup("individual_local");
										char* subnet_option =strdup(" --subnet ");
										subnet_definition = dcat_and_free(&subnet_definition, &subnet_option, 1, 1);
										subnet_definition = dcat_and_free(&subnet_definition, &local_subnet, 1, 0);
									}
								}
							}
							
							//insert rule
							run_shell_command(dynamic_strcat(14, "iptables -t ", quota_table, " -A ", chains[type_index], ip_test, " -m bandwidth --id \"", type_id, "\" --type ", applies_to, subnet_definition, " --greater_than ", limit, reset, set_death_mark), 1);
							//restore from backup
							if(do_restore)
							{
								restore_backup_for_id(type_id, "/usr/data/quotas");
							}
							
							free(applies_to);
							free(ip_test);
							free(type_id);
							free(limit);
						}
					}
				}
				free(ip);
			}
			free(next_quota);

		}

		run_shell_command(dynamic_strcat(3,"iptables -t ", quota_table, " -A egress_quotas -j CONNMARK --set-mark 0x0/0xFF000000 2>/dev/null"), 1);
		run_shell_command(dynamic_strcat(3,"iptables -t ", quota_table, " -A ingress_quotas -j CONNMARK --set-mark 0x0/0xFF000000 2>/dev/null"), 1);
		run_shell_command(dynamic_strcat(3,"iptables -t ", quota_table, " -A combined_quotas -j CONNMARK --set-mark 0x0/0xFF000000 2>/dev/null"), 1);
		run_shell_command(dynamic_strcat(5,"iptables -t filter -I FORWARD -m connmark --mark ", death_mark, "/", death_mask, " -j REJECT 2>/dev/null"), 1);

		//make sure crontab is up to date

	}
	else
	{
		//remove crontab if it exists
	}

	uci_free_context(ctx);

	return 0;
}

void restore_backup_for_id(char* id, char* quota_backup_dir)
{
	char* quota_file_name;
	if(strstr(id, "/") != NULL)
	{
		char* quota_file_name = dynamic_replace(id, "/", "_");
	}
	else
	{
		quota_file_name = strdup(id);
	}

	char* quota_file_path = dynamic_strcat(3, quota_backup_dir, "/quota_", quota_file_name);
	FILE* in_file = fopen(quota_file_path, "r");
	free(quota_file_path);
	
	if(in_file != NULL)
	{
		unsigned long num_data_parts = 0;
		char* file_data = read_entire_file(in_file, 4086, &num_data_parts);
		char whitespace[] =  {'\n', '\r', '\t', ' '};
		char** data_parts = split_on_separators(file_data, whitespace, 4, -1, 0, &num_data_parts);
		free(file_data);

		time_t last_backup = 0;
		unsigned long num_ips = num_data_parts/2;
       		ip_bw* buffer = (ip_bw*)malloc(BANDWIDTH_ENTRY_LENGTH*num_ips);
		unsigned long data_index = 0;
		unsigned long buffer_index = 0;
		while(data_index < num_data_parts)
		{
			ip_bw next;
			struct in_addr ipaddr;
			int valid = inet_aton(data_parts[data_index], &ipaddr);
			if(!valid)
			{
				sscanf(data_parts[data_index], "%ld", &last_backup);
			}
			data_index++;

			if(valid && data_index < num_data_parts)
			{
				next.ip = ipaddr.s_addr;
				valid = sscanf(data_parts[data_index], "%lld", (long long int*)&(next.bw) );
				data_index++;
			}
			else
			{
				valid = 0;
			}

			if(valid)
			{
				/* printf("ip=%d, bw=%lld\n", next.ip, (long long int)next.bw); */
				buffer[buffer_index] = next;
				buffer_index++;
			}
		}
		set_bandwidth_usage_for_rule_id(id, num_ips, last_backup, buffer, 1000);


		fclose(in_file);
	}
}

void delete_chain_from_table(char* table, char* delete_chain)
{	
	char *command = dynamic_strcat(3, "iptables -t ", table, " -L --line-numbers 2>/dev/null");
	unsigned long num_lines = 0;
	char** table_dump = get_shell_command_output_lines(command, &num_lines, 1);
	

	unsigned long line_index;
	char* current_chain = NULL;
	list* delete_commands = initialize_list();


	for(line_index=0; line_index < num_lines; line_index++)
	{
		char* line = table_dump[line_index];
		unsigned long num_pieces = 0;
		char whitespace[] = { '\t', ' ', '\r', '\n' };
		char** line_pieces = split_on_separators(line, whitespace, 4, -1, 0, &num_pieces);
		if(strcmp(line_pieces[0], "Chain") == 0)
		{
			if(current_chain != NULL) { free(current_chain); }
			current_chain = strdup(line_pieces[1]);
		}
		else 
		{
			unsigned long line_num;
			int read = sscanf(line_pieces[0], "%ld", &line_num);

			if(read > 0 && current_chain != NULL && num_pieces >1)
			{
				if(strcmp(line_pieces[1], delete_chain) == 0)
				{
					char* delete_command = dynamic_strcat(7, "iptables -t ", table, " -D ", current_chain, " ", line_pieces[0], " 2>/dev/null");
					push_list(delete_commands, delete_command);
				}
			}
		}

		//free line_pieces
		free_null_terminated_string_array(line_pieces);
	}
	free_null_terminated_string_array(table_dump);
	
	/* final two commands to flush chain being deleted and whack it */
	unshift_list(delete_commands, dynamic_strcat(5, "iptables -t ", table, " -F ", delete_chain, " 2>/dev/null"));
	unshift_list(delete_commands, dynamic_strcat(5, "iptables -t ", table, " -X ", delete_chain, " 2>/dev/null"));

	/* run delete commands */
	while(delete_commands->length > 0)
	{
		char *next_command = (char*)pop_list(delete_commands);
		run_shell_command(next_command, 1);
	}

}

void run_shell_command(char* command, int free_command_str)
{
	system(command);
	if(free_command_str)
	{
		free(command);
	}
}

char** get_shell_command_output_lines(char* command, unsigned long* num_lines, int free_command)
{
	char** ret = NULL;
	FILE* shell_out = popen(command, "r");
	if(shell_out != NULL)
	{
		char linebreaks[] = { '\n', '\r' };
		unsigned long read_length;
		char* all_data = (char*)read_entire_file(shell_out, 2048, &read_length);
		ret = split_on_separators(all_data, linebreaks, 2, -1, 0, num_lines);
		free(all_data);
		fclose(shell_out);
	}
	if(free_command)
	{
		free(command);
	}
	return ret;
}

void free_null_terminated_string_array(char** strings)
{
	int index;
	for(index = 0; strings[index] != NULL; index++)
	{
		free(strings[index]);
	}
	free(strings);
}



list* get_all_sections_of_type(struct uci_context *ctx, char* package, char* section_type)
{

	struct uci_package *p = NULL;
	struct uci_element *e = NULL;

	list* sections_of_type = initialize_list();
	if(uci_load(ctx, package, &p) == UCI_OK)
	{
		uci_foreach_element( &p->sections, e)
		{
			struct uci_section *section = uci_to_section(e);
			if(safe_strcmp(section->type, section_type) == 0)
			{
				push_list(sections_of_type, strdup(section->e.name));
			}
		}
	}
	return sections_of_type;
}


char* get_uci_option(struct uci_context* ctx, char* package_name, char* section_name, char* option_name)
{
	char* option_value = NULL;
	struct uci_ptr ptr;
	char* lookup_str = dynamic_strcat(5, package_name, ".", section_name, ".", option_name);
	int ret_value = uci_lookup_ptr(ctx, &ptr, lookup_str, 1);
	if(ret_value == UCI_OK)
	{
		if( !(ptr.flags & UCI_LOOKUP_COMPLETE))
		{
			ret_value = UCI_ERR_NOTFOUND;
		}
		else
		{
			struct uci_element *e = (struct uci_element*)ptr.o;
			option_value = get_option_value_string(uci_to_option(e));
		}
	}
	free(lookup_str);

	return option_value;
}




// this function dynamically allocates memory for
// the option string, but since this program exits
// almost immediately (after printing variable info)
// the massive memory leak we're opening up shouldn't
// cause any problems.  This is your reminder/warning
// that this might be an issue if you use this code to
// do anything fancy.
char* get_option_value_string(struct uci_option* uopt)
{
	char* opt_str = NULL;
	if(uopt->type == UCI_TYPE_STRING)
	{
		opt_str = strdup(uopt->v.string);
	}
	if(uopt->type == UCI_TYPE_LIST)
	{
		struct uci_element* e;
		uci_foreach_element(&uopt->v.list, e)
		{
			if(opt_str == NULL)
			{
				opt_str = strdup(e->name);
			}
			else
			{
				char* tmp;
				tmp = dynamic_strcat(3, opt_str, " ", e->name);
				free(opt_str);
				opt_str = tmp;
			}
		}
	}

	return opt_str;
}


