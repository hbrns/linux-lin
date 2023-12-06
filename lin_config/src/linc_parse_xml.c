#include <libxml/parser.h>
#include <assert.h>
#include <string.h>
#include <errno.h>
#include "lin_config.h"

/**
* @val -- ptr to int where the int value will be saved
* @cur -- xmlNodePtr
* @str -- Property name (string)
*/
static inline int linc_xml_get_prop_int(int *val, xmlNodePtr cur, const xmlChar* str)
{
	xmlChar *attr;
	char *endptr;

	attr = xmlGetProp(cur, str);
	if (!attr)
		assert(0);

	errno = 0;
	*val = strtol((const char *)attr, &endptr, 10);
	if ((char *)attr == endptr)
		return -1;
	if (errno != 0) {
		perror("strtol()");
		return -1;
	}

	xmlFree(attr);
	return 0;
}

/**
* @val -- ptr to int where the int value will be saved
* @doc -- xmlDocPtr
* @cur -- xmlNodePtr
*/
static inline int linc_xml_get_element_int(int *val, xmlDocPtr doc, xmlNodePtr cur)
{
	xmlChar *key;
	char *endptr;

	key = xmlNodeListGetString(doc, cur->children, 1);
	if (!key)
		assert(0);

	errno = 0;
	*val = strtol((const char *)key, &endptr, 10);
	if ((char *)key == endptr)
		return -1;
	if (errno != 0) {
		perror("strtol()");
		return -1;
	}
	xmlFree(key);

	return 0;
}

int linc_parse_scheduler_entries(struct linc_lin_state *linc_lin_state, xmlDocPtr doc, xmlNodePtr cur)
{
	cur = cur->children;
	while (cur) {
		if (!xmlStrcmp(cur->name, (const xmlChar *)"Entry")) {
			int ret;
			int linid;
			int interval;

			ret = linc_xml_get_element_int(&linid, doc, cur);
			if (ret < 0)
				return ret;

			ret = linc_xml_get_prop_int(&interval, cur, (const xmlChar *)"Time");
			if (ret < 0)
				return ret;

			linc_lin_state->scheduler_entry[linc_lin_state->scheduler_entries_cnt].lin_id = linid;
			linc_lin_state->scheduler_entry[linc_lin_state->scheduler_entries_cnt].interval_ms = interval;
			linc_lin_state->scheduler_entries_cnt++;

			//printf("Time = %d Lin ID = %d Entry no. = %d\n",
			//	interval, linid, linc_lin_state->scheduler_entries_cnt-1);
		}
		cur = cur->next;
	}
	return 0;
}

int linc_parse_frame_configuration(struct linc_lin_state *linc_lin_state, xmlDocPtr doc, xmlNodePtr cur)
{
	xmlNodePtr tmp_node;
	int val;

	cur = cur->children;
	while (cur) {
		if (!xmlStrcmp(cur->name, (const xmlChar *)"Frame")) {
			tmp_node = cur->children;
			/* We are able to write into the main Configuration array after
			parsing of all necessary elements (especially LIN ID) -- store
			parsed elements in this temporary entry -- copy the entry afterwards */
			struct linc_frame_entry tmp_fr_entry;
			int linid = -1;
			int ret;

			while (tmp_node) {
				if (!xmlStrcmp(tmp_node->name, (const xmlChar *)"ID")) {
					ret = linc_xml_get_element_int(&linid, doc, tmp_node);
					//printf("ID = %d\n", linid);
					if (ret < 0)
						return ret;
				}
				if (!xmlStrcmp(tmp_node->name, (const xmlChar *)"Length")) {
					ret = linc_xml_get_element_int(&val, doc, tmp_node);
					if (ret < 0)
						return ret;

					tmp_fr_entry.data_len = val;
					//printf("Length = %d\n", val);
				}
				if (!xmlStrcmp(tmp_node->name, (const xmlChar *)"Active")) {
					ret = linc_xml_get_element_int(&val, doc, tmp_node);
					if (ret < 0)
						return ret;

					tmp_fr_entry.status = val;
					//printf("Active = %d\n", val);
				}
				if (!xmlStrcmp(tmp_node->name, (const xmlChar *)"Data")) {
					int indx = 0;
					xmlNodePtr tmp_node2;
					tmp_node2 = tmp_node->children;
					while (tmp_node2) {
						if (!xmlStrcmp(tmp_node2->name, (const xmlChar *)"Byte")) {
							// Byte indexing in XML file is wrong
							//ret = linc_xml_get_prop_int(&index, tmp_node2,
							//	(const xmlChar *)"Index");
							ret = linc_xml_get_element_int(&val, doc, tmp_node2);
							if (ret < 0)
								return ret;

							//printf("Data = %d\n", val);
							snprintf((char *)&tmp_fr_entry.data[indx], 1, "%i", val);
							indx++;
						}
						tmp_node2 = tmp_node2->next;
					}
				}
				tmp_node = tmp_node->next;
			}

			if (linid >= 0 && linid <= MAX_LIN_ID) {
				memcpy(&linc_lin_state->frame_entry[linid], &tmp_fr_entry,
					sizeof(struct linc_frame_entry));
			}
		}
		cur = cur->next;
	}

	return 0;
}

int linc_parse_configuration(char *filename, struct linc_lin_state *linc_lin_state)
{
	int val;
	int ret;
	xmlDocPtr doc;
	xmlNodePtr cur_node;

	if (!filename)
		filename = PCL_DEFAULT_CONFIG;

	xmlKeepBlanksDefault(1);
	doc = xmlParseFile(filename);
	if (doc == NULL)
		return -1;

	cur_node = xmlDocGetRootElement(doc);
	if (cur_node == NULL) {
		fprintf(stderr, "Configuration file %s is empty\n", filename);
		xmlFreeDoc(doc);
		return -1;
	}

	/* Check for Root element */
	if (xmlStrcmp(cur_node->name, (const xmlChar *)"PCLIN_PROFILE"))
		goto exit_failure;

	/* Check for LIN element */
	cur_node = cur_node->children;
	while (cur_node) {
		if (!xmlStrcmp(cur_node->name, (const xmlChar *)"LIN"))
			break;

		cur_node = cur_node->next;
	}

	if (!cur_node)
		goto exit_failure;

	/* Process LIN configuration */
	cur_node = cur_node->children;
	while (cur_node) {
		if (!xmlStrcmp(cur_node->name, (const xmlChar *)"Active")) {
			ret = linc_xml_get_element_int(&val, doc, cur_node);
			if (ret < 0)
				goto exit_failure;

			linc_lin_state->is_active = val;
		}
		if (!xmlStrcmp(cur_node->name, (const xmlChar *)"Baudrate")) {
			ret = linc_xml_get_element_int(&val, doc, cur_node);
			if (ret < 0)
				goto exit_failure;

			linc_lin_state->baudrate = val;
		}
		if (!xmlStrcmp(cur_node->name, (const xmlChar *)"Master_Status")) {
			ret = linc_xml_get_element_int(&val, doc, cur_node);
			if (ret < 0)
				goto exit_failure;

			linc_lin_state->master_status = val;
		}
		if (!xmlStrcmp(cur_node->name, (const xmlChar *)"Bus_Termination")) {
			ret = linc_xml_get_element_int(&val, doc, cur_node);
			if (ret < 0)
				goto exit_failure;

			linc_lin_state->bus_termination = val;
		}
		if (!xmlStrcmp(cur_node->name, (const xmlChar *)"Scheduler_Entries")) {
			ret = linc_parse_scheduler_entries(linc_lin_state, doc, cur_node);
			if (ret < 0)
				goto exit_failure;
		}
		if (!xmlStrcmp(cur_node->name, (const xmlChar *)"Frame_Configuration")) {
			ret = linc_parse_frame_configuration(linc_lin_state, doc, cur_node);
			if (ret < 0)
				goto exit_failure;
		}

		cur_node = cur_node->next;
	}

	xmlFreeDoc(doc);
	return 0;

exit_failure:
	fprintf(stderr, "Invalid configuration file\n");
	xmlFreeDoc(doc);
	return -1;
}

