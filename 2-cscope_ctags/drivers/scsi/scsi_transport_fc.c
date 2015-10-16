/*
 *  FiberChannel transport specific attributes exported to sysfs.
 *
 *  Copyright (c) 2003 Silicon Graphics, Inc.  All rights reserved.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 *  ========
 *
 *  Copyright (C) 2004-2007   James Smart, Emulex Corporation
 *    Rewrite for host, target, device, and remote port attributes,
 *    statistics, and service functions...
 *    Add vports, etc
 *
 */
#include <linux/module.h>
#include <linux/init.h>
#include <scsi/scsi_device.h>
#include <scsi/scsi_host.h>
#include <scsi/scsi_transport.h>
#include <scsi/scsi_transport_fc.h>
#include <scsi/scsi_cmnd.h>
#include <linux/netlink.h>
#include <net/netlink.h>
#include <scsi/scsi_netlink_fc.h>
#include <scsi/scsi_bsg_fc.h>
#include "scsi_priv.h"
#include "scsi_transport_fc_internal.h"

static int fc_queue_work(struct Scsi_Host *, struct work_struct *);
static void fc_vport_sched_delete(struct work_struct *work);
static int fc_vport_setup(struct Scsi_Host *shost, int channel,
	struct device *pdev, struct fc_vport_identifiers  *ids,
	struct fc_vport **vport);
static int fc_bsg_hostadd(struct Scsi_Host *, struct fc_host_attrs *);
static int fc_bsg_rportadd(struct Scsi_Host *, struct fc_rport *);
static void fc_bsg_remove(struct request_queue *);
static void fc_bsg_goose_queue(struct fc_rport *);

/*
 * Redefine so that we can have same named attributes in the
 * sdev/starget/host objects.
 */
#define FC_DEVICE_ATTR(_prefix,_name,_mode,_show,_store)		\
struct device_attribute device_attr_##_prefix##_##_name = 	\
	__ATTR(_name,_mode,_show,_store)

#define fc_enum_name_search(title, table_type, table)			\
static const char *get_fc_##title##_name(enum table_type table_key)	\
{									\
	int i;								\
	char *name = NULL;						\
									\
	for (i = 0; i < ARRAY_SIZE(table); i++) {			\
		if (table[i].value == table_key) {			\
			name = table[i].name;				\
			break;						\
		}							\
	}								\
	return name;							\
}

#define fc_enum_name_match(title, table_type, table)			\
static int get_fc_##title##_match(const char *table_key,		\
		enum table_type *value)					\
{									\
	int i;								\
									\
	for (i = 0; i < ARRAY_SIZE(table); i++) {			\
		if (strncmp(table_key, table[i].name,			\
				table[i].matchlen) == 0) {		\
			*value = table[i].value;			\
			return 0; /* success */				\
		}							\
	}								\
	return 1; /* failure */						\
}


/* Convert fc_port_type values to ascii string name */
static struct {
	enum fc_port_type	value;
	char			*name;
} fc_port_type_names[] = {
	{ FC_PORTTYPE_UNKNOWN,		"Unknown" },
	{ FC_PORTTYPE_OTHER,		"Other" },
	{ FC_PORTTYPE_NOTPRESENT,	"Not Present" },
	{ FC_PORTTYPE_NPORT,	"NPort (fabric via point-to-point)" },
	{ FC_PORTTYPE_NLPORT,	"NLPort (fabric via loop)" },
	{ FC_PORTTYPE_LPORT,	"LPort (private loop)" },
	{ FC_PORTTYPE_PTP,	"Point-To-Point (direct nport connection)" },
	{ FC_PORTTYPE_NPIV,		"NPIV VPORT" },
};
fc_enum_name_search(port_type, fc_port_type, fc_port_type_names)
#define FC_PORTTYPE_MAX_NAMELEN		50

/* Reuse fc_port_type enum function for vport_type */
#define get_fc_vport_type_name get_fc_port_type_name


/* Convert fc_host_event_code values to ascii string name */
static const struct {
	enum fc_host_event_code		value;
	char				*name;
} fc_host_event_code_names[] = {
	{ FCH_EVT_LIP,			"lip" },
	{ FCH_EVT_LINKUP,		"link_up" },
	{ FCH_EVT_LINKDOWN,		"link_down" },
	{ FCH_EVT_LIPRESET,		"lip_reset" },
	{ FCH_EVT_RSCN,			"rscn" },
	{ FCH_EVT_ADAPTER_CHANGE,	"adapter_chg" },
	{ FCH_EVT_PORT_UNKNOWN,		"port_unknown" },
	{ FCH_EVT_PORT_ONLINE,		"port_online" },
	{ FCH_EVT_PORT_OFFLINE,		"port_offline" },
	{ FCH_EVT_PORT_FABRIC,		"port_fabric" },
	{ FCH_EVT_LINK_UNKNOWN,		"link_unknown" },
	{ FCH_EVT_VENDOR_UNIQUE,	"vendor_unique" },
};
fc_enum_name_search(host_event_code, fc_host_event_code,
		fc_host_event_code_names)
#define FC_HOST_EVENT_CODE_MAX_NAMELEN	30


/* Convert fc_port_state values to ascii string name */
static struct {
	enum fc_port_state	value;
	char			*name;
} fc_port_state_names[] = {
	{ FC_PORTSTATE_UNKNOWN,		"Unknown" },
	{ FC_PORTSTATE_NOTPRESENT,	"Not Present" },
	{ FC_PORTSTATE_ONLINE,		"Online" },
	{ FC_PORTSTATE_OFFLINE,		"Offline" },
	{ FC_PORTSTATE_BLOCKED,		"Blocked" },
	{ FC_PORTSTATE_BYPASSED,	"Bypassed" },
	{ FC_PORTSTATE_DIAGNOSTICS,	"Diagnostics" },
	{ FC_PORTSTATE_LINKDOWN,	"Linkdown" },
	{ FC_PORTSTATE_ERROR,		"Error" },
	{ FC_PORTSTATE_LOOPBACK,	"Loopback" },
	{ FC_PORTSTATE_DELETED,		"Deleted" },
};
fc_enum_name_search(port_state, fc_port_state, fc_port_state_names)
#define FC_PORTSTATE_MAX_NAMELEN	20


/* Convert fc_vport_state values to ascii string name */
static struct {
	enum fc_vport_state	value;
	char			*name;
} fc_vport_state_names[] = {
	{ FC_VPORT_UNKNOWN,		"Unknown" },
	{ FC_VPORT_ACTIVE,		"Active" },
	{ FC_VPORT_DISABLED,		"Disabled" },
	{ FC_VPORT_LINKDOWN,		"Linkdown" },
	{ FC_VPORT_INITIALIZING,	"Initializing" },
	{ FC_VPORT_NO_FABRIC_SUPP,	"No Fabric Support" },
	{ FC_VPORT_NO_FABRIC_RSCS,	"No Fabric Resources" },
	{ FC_VPORT_FABRIC_LOGOUT,	"Fabric Logout" },
	{ FC_VPORT_FABRIC_REJ_WWN,	"Fabric Rejected WWN" },
	{ FC_VPORT_FAILED,		"VPort Failed" },
};
fc_enum_name_search(vport_state, fc_vport_state, fc_vport_state_names)
#define FC_VPORTSTATE_MAX_NAMELEN	24

/* Reuse fc_vport_state enum function for vport_last_state */
#define get_fc_vport_last_state_name get_fc_vport_state_name


/* Convert fc_tgtid_binding_type values to ascii string name */
static const struct {
	enum fc_tgtid_binding_type	value;
	char				*name;
	int				matchlen;
} fc_tgtid_binding_type_names[] = {
	{ FC_TGTID_BIND_NONE, "none", 4 },
	{ FC_TGTID_BIND_BY_WWPN, "wwpn (World Wide Port Name)", 4 },
	{ FC_TGTID_BIND_BY_WWNN, "wwnn (World Wide Node Name)", 4 },
	{ FC_TGTID_BIND_BY_ID, "port_id (FC Address)", 7 },
};
fc_enum_name_search(tgtid_bind_type, fc_tgtid_binding_type,
		fc_tgtid_binding_type_names)
fc_enum_name_match(tgtid_bind_type, fc_tgtid_binding_type,
		fc_tgtid_binding_type_names)
#define FC_BINDTYPE_MAX_NAMELEN	30


#define fc_bitfield_name_search(title, table)			\
static ssize_t							\
get_fc_##title##_names(u32 table_key, char *buf)		\
{								\
	char *prefix = "";					\
	ssize_t len = 0;					\
	int i;							\
								\
	for (i = 0; i < ARRAY_SIZE(table); i++) {		\
		if (table[i].value & table_key) {		\
			len += sprintf(buf + len, "%s%s",	\
				prefix, table[i].name);		\
			prefix = ", ";				\
		}						\
	}							\
	len += sprintf(buf + len, "\n");			\
	return len;						\
}


/* Convert FC_COS bit values to ascii string name */
static const struct {
	u32 			value;
	char			*name;
} fc_cos_names[] = {
	{ FC_COS_CLASS1,	"Class 1" },
	{ FC_COS_CLASS2,	"Class 2" },
	{ FC_COS_CLASS3,	"Class 3" },
	{ FC_COS_CLASS4,	"Class 4" },
	{ FC_COS_CLASS6,	"Class 6" },
};
fc_bitfield_name_search(cos, fc_cos_names)


/* Convert FC_PORTSPEED bit values to ascii string name */
static const struct {
	u32 			value;
	char			*name;
} fc_port_speed_names[] = {
	{ FC_PORTSPEED_1GBIT,		"1 Gbit" },
	{ FC_PORTSPEED_2GBIT,		"2 Gbit" },
	{ FC_PORTSPEED_4GBIT,		"4 Gbit" },
	{ FC_PORTSPEED_10GBIT,		"10 Gbit" },
	{ FC_PORTSPEED_8GBIT,		"8 Gbit" },
	{ FC_PORTSPEED_16GBIT,		"16 Gbit" },
	{ FC_PORTSPEED_NOT_NEGOTIATED,	"Not Negotiated" },
};
fc_bitfield_name_search(port_speed, fc_port_speed_names)


static int
show_fc_fc4s (char *buf, u8 *fc4_list)
{
	int i, len=0;

	for (i = 0; i < FC_FC4_LIST_SIZE; i++, fc4_list++)
		len += sprintf(buf + len , "0x%02x ", *fc4_list);
	len += sprintf(buf + len, "\n");
	return len;
}


/* Convert FC_PORT_ROLE bit values to ascii string name */
static const struct {
	u32 			value;
	char			*name;
} fc_port_role_names[] = {
	{ FC_PORT_ROLE_FCP_TARGET,	"FCP Target" },
	{ FC_PORT_ROLE_FCP_INITIATOR,	"FCP Initiator" },
	{ FC_PORT_ROLE_IP_PORT,		"IP Port" },
};
fc_bitfield_name_search(port_roles, fc_port_role_names)

/*
 * Define roles that are specific to port_id. Values are relative to ROLE_MASK.
 */
#define FC_WELLKNOWN_PORTID_MASK	0xfffff0
#define FC_WELLKNOWN_ROLE_MASK  	0x00000f
#define FC_FPORT_PORTID			0x00000e
#define FC_FABCTLR_PORTID		0x00000d
#define FC_DIRSRVR_PORTID		0x00000c
#define FC_TIMESRVR_PORTID		0x00000b
#define FC_MGMTSRVR_PORTID		0x00000a


static void fc_timeout_deleted_rport(struct work_struct *work);
static void fc_timeout_fail_rport_io(struct work_struct *work);
static void fc_scsi_scan_rport(struct work_struct *work);

/*
 * Attribute counts pre object type...
 * Increase these values if you add attributes
 */
#define FC_STARGET_NUM_ATTRS 	3
#define FC_RPORT_NUM_ATTRS	10
#define FC_VPORT_NUM_ATTRS	9
#define FC_HOST_NUM_ATTRS	22

struct fc_internal {
	struct scsi_transport_template t;
	struct fc_function_template *f;

	/*
	 * For attributes : each object has :
	 *   An array of the actual attributes structures
	 *   An array of null-terminated pointers to the attribute
	 *     structures - used for mid-layer interaction.
	 *
	 * The attribute containers for the starget and host are are
	 * part of the midlayer. As the remote port is specific to the
	 * fc transport, we must provide the attribute container.
	 */
	struct device_attribute private_starget_attrs[
							FC_STARGET_NUM_ATTRS];
	struct device_attribute *starget_attrs[FC_STARGET_NUM_ATTRS + 1];

	struct device_attribute private_host_attrs[FC_HOST_NUM_ATTRS];
	struct device_attribute *host_attrs[FC_HOST_NUM_ATTRS + 1];

	struct transport_container rport_attr_cont;
	struct device_attribute private_rport_attrs[FC_RPORT_NUM_ATTRS];
	struct device_attribute *rport_attrs[FC_RPORT_NUM_ATTRS + 1];

	struct transport_container vport_attr_cont;
	struct device_attribute private_vport_attrs[FC_VPORT_NUM_ATTRS];
	struct device_attribute *vport_attrs[FC_VPORT_NUM_ATTRS + 1];
};

#define to_fc_internal(tmpl)	container_of(tmpl, struct fc_internal, t)

static int fc_target_setup(struct transport_container *tc, struct device *dev,
			   struct device *cdev)
{
	struct scsi_target *starget = to_scsi_target(dev);
	struct fc_rport *rport = starget_to_rport(starget);

	/*
	 * if parent is remote port, use values from remote port.
	 * Otherwise, this host uses the fc_transport, but not the
	 * remote port interface. As such, initialize to known non-values.
	 */
	if (rport) {
		fc_starget_node_name(starget) = rport->node_name;
		fc_starget_port_name(starget) = rport->port_name;
		fc_starget_port_id(starget) = rport->port_id;
	} else {
		fc_starget_node_name(starget) = -1;
		fc_starget_port_name(starget) = -1;
		fc_starget_port_id(starget) = -1;
	}

	return 0;
}

static DECLARE_TRANSPORT_CLASS(fc_transport_class,
			       "fc_transport",
			       fc_target_setup,
			       NULL,
			       NULL);

static int fc_host_setup(struct transport_container *tc, struct device *dev,
			 struct device *cdev)
{
	struct Scsi_Host *shost = dev_to_shost(dev);
	struct fc_host_attrs *fc_host = shost_to_fc_host(shost);

	/*
	 * Set default values easily detected by the midlayer as
	 * failure cases.  The scsi lldd is responsible for initializing
	 * all transport attributes to valid values per host.
	 */
	fc_host->node_name = -1;
	fc_host->port_name = -1;
	fc_host->permanent_port_name = -1;
	fc_host->supported_classes = FC_COS_UNSPECIFIED;
	memset(fc_host->supported_fc4s, 0,
		sizeof(fc_host->supported_fc4s));
	fc_host->supported_speeds = FC_PORTSPEED_UNKNOWN;
	fc_host->maxframe_size = -1;
	fc_host->max_npiv_vports = 0;
	memset(fc_host->serial_number, 0,
		sizeof(fc_host->serial_number));

	fc_host->port_id = -1;
	fc_host->port_type = FC_PORTTYPE_UNKNOWN;
	fc_host->port_state = FC_PORTSTATE_UNKNOWN;
	memset(fc_host->active_fc4s, 0,
		sizeof(fc_host->active_fc4s));
	fc_host->speed = FC_PORTSPEED_UNKNOWN;
	fc_host->fabric_name = -1;
	memset(fc_host->symbolic_name, 0, sizeof(fc_host->symbolic_name));
	memset(fc_host->system_hostname, 0, sizeof(fc_host->system_hostname));

	fc_host->tgtid_bind_type = FC_TGTID_BIND_BY_WWPN;

	INIT_LIST_HEAD(&fc_host->rports);
	INIT_LIST_HEAD(&fc_host->rport_bindings);
	INIT_LIST_HEAD(&fc_host->vports);
	fc_host->next_rport_number = 0;
	fc_host->next_target_id = 0;
	fc_host->next_vport_number = 0;
	fc_host->npiv_vports_inuse = 0;

	snprintf(fc_host->work_q_name, sizeof(fc_host->work_q_name),
		 "fc_wq_%d", shost->host_no);
	fc_host->work_q = create_singlethread_workqueue(
					fc_host->work_q_name);
	if (!fc_host->work_q)
		return -ENOMEM;

	snprintf(fc_host->devloss_work_q_name,
		 sizeof(fc_host->devloss_work_q_name),
		 "fc_dl_%d", shost->host_no);
	fc_host->devloss_work_q = create_singlethread_workqueue(
					fc_host->devloss_work_q_name);
	if (!fc_host->devloss_work_q) {
		destroy_workqueue(fc_host->work_q);
		fc_host->work_q = NULL;
		return -ENOMEM;
	}

	fc_bsg_hostadd(shost, fc_host);
	/* ignore any bsg add error - we just can't do sgio */

	return 0;
}

static int fc_host_remove(struct transport_container *tc, struct device *dev,
			 struct device *cdev)
{
	struct Scsi_Host *shost = dev_to_shost(dev);
	struct fc_host_attrs *fc_host = shost_to_fc_host(shost);

	fc_bsg_remove(fc_host->rqst_q);
	return 0;
}

static DECLARE_TRANSPORT_CLASS(fc_host_class,
			       "fc_host",
			       fc_host_setup,
			       fc_host_remove,
			       NULL);

/*
 * Setup and Remove actions for remote ports are handled
 * in the service functions below.
 */
static DECLARE_TRANSPORT_CLASS(fc_rport_class,
			       "fc_remote_ports",
			       NULL,
			       NULL,
			       NULL);

/*
 * Setup and Remove actions for virtual ports are handled
 * in the service functions below.
 */
static DECLARE_TRANSPORT_CLASS(fc_vport_class,
			       "fc_vports",
			       NULL,
			       NULL,
			       NULL);

/*
 * Module Parameters
 */

/*
 * dev_loss_tmo: the default number of seconds that the FC transport
 *   should insulate the loss of a remote port.
 *   The maximum will be capped by the value of SCSI_DEVICE_BLOCK_MAX_TIMEOUT.
 */
static unsigned int fc_dev_loss_tmo = 60;		/* seconds */

module_param_named(dev_loss_tmo, fc_dev_loss_tmo, uint, S_IRUGO|S_IWUSR);
MODULE_PARM_DESC(dev_loss_tmo,
		 "Maximum number of seconds that the FC transport should"
		 " insulate the loss of a remote port. Once this value is"
		 " exceeded, the scsi target is removed. Value should be"
		 " between 1 and SCSI_DEVICE_BLOCK_MAX_TIMEOUT.");

/*
 * Netlink Infrastructure
 */

static atomic_t fc_event_seq;

/**
 * fc_get_event_number - Obtain the next sequential FC event number
 *
 * Notes:
 *   We could have inlined this, but it would have required fc_event_seq to
 *   be exposed. For now, live with the subroutine call.
 *   Atomic used to avoid lock/unlock...
 */
u32
fc_get_event_number(void)
{
	return atomic_add_return(1, &fc_event_seq);
}
EXPORT_SYMBOL(fc_get_event_number);


/**
 * fc_host_post_event - called to post an even on an fc_host.
 * @shost:		host the event occurred on
 * @event_number:	fc event number obtained from get_fc_event_number()
 * @event_code:		fc_host event being posted
 * @event_data:		32bits of data for the event being posted
 *
 * Notes:
 *	This routine assumes no locks are held on entry.
 */
void
fc_host_post_event(struct Scsi_Host *shost, u32 event_number,
		enum fc_host_event_code event_code, u32 event_data)
{
	struct sk_buff *skb;
	struct nlmsghdr	*nlh;
	struct fc_nl_event *event;
	const char *name;
	u32 len, skblen;
	int err;

	if (!scsi_nl_sock) {
		err = -ENOENT;
		goto send_fail;
	}

	len = FC_NL_MSGALIGN(sizeof(*event));
	skblen = NLMSG_SPACE(len);

	skb = alloc_skb(skblen, GFP_KERNEL);
	if (!skb) {
		err = -ENOBUFS;
		goto send_fail;
	}

	nlh = nlmsg_put(skb, 0, 0, SCSI_TRANSPORT_MSG,
				skblen - sizeof(*nlh), 0);
	if (!nlh) {
		err = -ENOBUFS;
		goto send_fail_skb;
	}
	event = NLMSG_DATA(nlh);

	INIT_SCSI_NL_HDR(&event->snlh, SCSI_NL_TRANSPORT_FC,
				FC_NL_ASYNC_EVENT, len);
	event->seconds = get_seconds();
	event->vendor_id = 0;
	event->host_no = shost->host_no;
	event->event_datalen = sizeof(u32);	/* bytes */
	event->event_num = event_number;
	event->event_code = event_code;
	event->event_data = event_data;

	nlmsg_multicast(scsi_nl_sock, skb, 0, SCSI_NL_GRP_FC_EVENTS,
			GFP_KERNEL);
	return;

send_fail_skb:
	kfree_skb(skb);
send_fail:
	name = get_fc_host_event_code_name(event_code);
	printk(KERN_WARNING
		"%s: Dropped Event : host %d %s data 0x%08x - err %d\n",
		__func__, shost->host_no,
		(name) ? name : "<unknown>", event_data, err);
	return;
}
EXPORT_SYMBOL(fc_host_post_event);


/**
 * fc_host_post_vendor_event - called to post a vendor unique event on an fc_host
 * @shost:		host the event occurred on
 * @event_number:	fc event number obtained from get_fc_event_number()
 * @data_len:		amount, in bytes, of vendor unique data
 * @data_buf:		pointer to vendor unique data
 * @vendor_id:          Vendor id
 *
 * Notes:
 *	This routine assumes no locks are held on entry.
 */
void
fc_host_post_vendor_event(struct Scsi_Host *shost, u32 event_number,
		u32 data_len, char * data_buf, u64 vendor_id)
{
	struct sk_buff *skb;
	struct nlmsghdr	*nlh;
	struct fc_nl_event *event;
	u32 len, skblen;
	int err;

	if (!scsi_nl_sock) {
		err = -ENOENT;
		goto send_vendor_fail;
	}

	len = FC_NL_MSGALIGN(sizeof(*event) + data_len);
	skblen = NLMSG_SPACE(len);

	skb = alloc_skb(skblen, GFP_KERNEL);
	if (!skb) {
		err = -ENOBUFS;
		goto send_vendor_fail;
	}

	nlh = nlmsg_put(skb, 0, 0, SCSI_TRANSPORT_MSG,
				skblen - sizeof(*nlh), 0);
	if (!nlh) {
		err = -ENOBUFS;
		goto send_vendor_fail_skb;
	}
	event = NLMSG_DATA(nlh);

	INIT_SCSI_NL_HDR(&event->snlh, SCSI_NL_TRANSPORT_FC,
				FC_NL_ASYNC_EVENT, len);
	event->seconds = get_seconds();
	event->vendor_id = vendor_id;
	event->host_no = shost->host_no;
	event->event_datalen = data_len;	/* bytes */
	event->event_num = event_number;
	event->event_code = FCH_EVT_VENDOR_UNIQUE;
	memcpy(&event->event_data, data_buf, data_len);

	nlmsg_multicast(scsi_nl_sock, skb, 0, SCSI_NL_GRP_FC_EVENTS,
			GFP_KERNEL);
	return;

send_vendor_fail_skb:
	kfree_skb(skb);
send_vendor_fail:
	printk(KERN_WARNING
		"%s: Dropped Event : host %d vendor_unique - err %d\n",
		__func__, shost->host_no, err);
	return;
}
EXPORT_SYMBOL(fc_host_post_vendor_event);



static __init int fc_transport_init(void)
{
	int error;

	atomic_set(&fc_event_seq, 0);

	error = transport_class_register(&fc_host_class);
	if (error)
		return error;
	error = transport_class_register(&fc_vport_class);
	if (error)
		return error;
	error = transport_class_register(&fc_rport_class);
	if (error)
		return error;
	return transport_class_register(&fc_transport_class);
}

static void __exit fc_transport_exit(void)
{
	transport_class_unregister(&fc_transport_class);
	transport_class_unregister(&fc_rport_class);
	transport_class_unregister(&fc_host_class);
	transport_class_unregister(&fc_vport_class);
}

/*
 * FC Remote Port Attribute Management
 */

#define fc_rport_show_function(field, format_string, sz, cast)		\
static ssize_t								\
show_fc_rport_##field (struct device *dev, 				\
		       struct device_attribute *attr, char *buf)	\
{									\
	struct fc_rport *rport = transport_class_to_rport(dev);		\
	struct Scsi_Host *shost = rport_to_shost(rport);		\
	struct fc_internal *i = to_fc_internal(shost->transportt);	\
	if ((i->f->get_rport_##field) &&				\
	    !((rport->port_state == FC_PORTSTATE_BLOCKED) ||		\
	      (rport->port_state == FC_PORTSTATE_DELETED) ||		\
	      (rport->port_state == FC_PORTSTATE_NOTPRESENT)))		\
		i->f->get_rport_##field(rport);				\
	return snprintf(buf, sz, format_string, cast rport->field); 	\
}

#define fc_rport_store_function(field)					\
static ssize_t								\
store_fc_rport_##field(struct device *dev,				\
		       struct device_attribute *attr,			\
		       const char *buf,	size_t count)			\
{									\
	int val;							\
	struct fc_rport *rport = transport_class_to_rport(dev);		\
	struct Scsi_Host *shost = rport_to_shost(rport);		\
	struct fc_internal *i = to_fc_internal(shost->transportt);	\
	char *cp;							\
	if ((rport->port_state == FC_PORTSTATE_BLOCKED) ||		\
	    (rport->port_state == FC_PORTSTATE_DELETED) ||		\
	    (rport->port_state == FC_PORTSTATE_NOTPRESENT))		\
		return -EBUSY;						\
	val = simple_strtoul(buf, &cp, 0);				\
	if (*cp && (*cp != '\n'))					\
		return -EINVAL;						\
	i->f->set_rport_##field(rport, val);				\
	return count;							\
}

#define fc_rport_rd_attr(field, format_string, sz)			\
	fc_rport_show_function(field, format_string, sz, )		\
static FC_DEVICE_ATTR(rport, field, S_IRUGO,			\
			 show_fc_rport_##field, NULL)

#define fc_rport_rd_attr_cast(field, format_string, sz, cast)		\
	fc_rport_show_function(field, format_string, sz, (cast))	\
static FC_DEVICE_ATTR(rport, field, S_IRUGO,			\
			  show_fc_rport_##field, NULL)

#define fc_rport_rw_attr(field, format_string, sz)			\
	fc_rport_show_function(field, format_string, sz, )		\
	fc_rport_store_function(field)					\
static FC_DEVICE_ATTR(rport, field, S_IRUGO | S_IWUSR,		\
			show_fc_rport_##field,				\
			store_fc_rport_##field)


#define fc_private_rport_show_function(field, format_string, sz, cast)	\
static ssize_t								\
show_fc_rport_##field (struct device *dev, 				\
		       struct device_attribute *attr, char *buf)	\
{									\
	struct fc_rport *rport = transport_class_to_rport(dev);		\
	return snprintf(buf, sz, format_string, cast rport->field); 	\
}

#define fc_private_rport_rd_attr(field, format_string, sz)		\
	fc_private_rport_show_function(field, format_string, sz, )	\
static FC_DEVICE_ATTR(rport, field, S_IRUGO,			\
			 show_fc_rport_##field, NULL)

#define fc_private_rport_rd_attr_cast(field, format_string, sz, cast)	\
	fc_private_rport_show_function(field, format_string, sz, (cast)) \
static FC_DEVICE_ATTR(rport, field, S_IRUGO,			\
			  show_fc_rport_##field, NULL)


#define fc_private_rport_rd_enum_attr(title, maxlen)			\
static ssize_t								\
show_fc_rport_##title (struct device *dev,				\
		       struct device_attribute *attr, char *buf)	\
{									\
	struct fc_rport *rport = transport_class_to_rport(dev);		\
	const char *name;						\
	name = get_fc_##title##_name(rport->title);			\
	if (!name)							\
		return -EINVAL;						\
	return snprintf(buf, maxlen, "%s\n", name);			\
}									\
static FC_DEVICE_ATTR(rport, title, S_IRUGO,			\
			show_fc_rport_##title, NULL)


#define SETUP_RPORT_ATTRIBUTE_RD(field)					\
	i->private_rport_attrs[count] = device_attr_rport_##field; \
	i->private_rport_attrs[count].attr.mode = S_IRUGO;		\
	i->private_rport_attrs[count].store = NULL;			\
	i->rport_attrs[count] = &i->private_rport_attrs[count];		\
	if (i->f->show_rport_##field)					\
		count++

#define SETUP_PRIVATE_RPORT_ATTRIBUTE_RD(field)				\
	i->private_rport_attrs[count] = device_attr_rport_##field; \
	i->private_rport_attrs[count].attr.mode = S_IRUGO;		\
	i->private_rport_attrs[count].store = NULL;			\
	i->rport_attrs[count] = &i->private_rport_attrs[count];		\
	count++

#define SETUP_RPORT_ATTRIBUTE_RW(field)					\
	i->private_rport_attrs[count] = device_attr_rport_##field; \
	if (!i->f->set_rport_##field) {					\
		i->private_rport_attrs[count].attr.mode = S_IRUGO;	\
		i->private_rport_attrs[count].store = NULL;		\
	}								\
	i->rport_attrs[count] = &i->private_rport_attrs[count];		\
	if (i->f->show_rport_##field)					\
		count++

#define SETUP_PRIVATE_RPORT_ATTRIBUTE_RW(field)				\
{									\
	i->private_rport_attrs[count] = device_attr_rport_##field; \
	i->rport_attrs[count] = &i->private_rport_attrs[count];		\
	count++;							\
}


/* The FC Transport Remote Port Attributes: */

/* Fixed Remote Port Attributes */

fc_private_rport_rd_attr(maxframe_size, "%u bytes\n", 20);

static ssize_t
show_fc_rport_supported_classes (struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	struct fc_rport *rport = transport_class_to_rport(dev);
	if (rport->supported_classes == FC_COS_UNSPECIFIED)
		return snprintf(buf, 20, "unspecified\n");
	return get_fc_cos_names(rport->supported_classes, buf);
}
static FC_DEVICE_ATTR(rport, supported_classes, S_IRUGO,
		show_fc_rport_supported_classes, NULL);

/* Dynamic Remote Port Attributes */

/*
 * dev_loss_tmo attribute
 */
fc_rport_show_function(dev_loss_tmo, "%d\n", 20, )
static ssize_t
store_fc_rport_dev_loss_tmo(struct device *dev, struct device_attribute *attr,
			    const char *buf, size_t count)
{
	int val;
	struct fc_rport *rport = transport_class_to_rport(dev);
	struct Scsi_Host *shost = rport_to_shost(rport);
	struct fc_internal *i = to_fc_internal(shost->transportt);
	char *cp;
	if ((rport->port_state == FC_PORTSTATE_BLOCKED) ||
	    (rport->port_state == FC_PORTSTATE_DELETED) ||
	    (rport->port_state == FC_PORTSTATE_NOTPRESENT))
		return -EBUSY;
	val = simple_strtoul(buf, &cp, 0);
	if ((*cp && (*cp != '\n')) ||
	    (val < 0) || (val > SCSI_DEVICE_BLOCK_MAX_TIMEOUT))
		return -EINVAL;
	i->f->set_rport_dev_loss_tmo(rport, val);
	return count;
}
static FC_DEVICE_ATTR(rport, dev_loss_tmo, S_IRUGO | S_IWUSR,
		show_fc_rport_dev_loss_tmo, store_fc_rport_dev_loss_tmo);


/* Private Remote Port Attributes */

fc_private_rport_rd_attr_cast(node_name, "0x%llx\n", 20, unsigned long long);
fc_private_rport_rd_attr_cast(port_name, "0x%llx\n", 20, unsigned long long);
fc_private_rport_rd_attr(port_id, "0x%06x\n", 20);

static ssize_t
show_fc_rport_roles (struct device *dev, struct device_attribute *attr,
		     char *buf)
{
	struct fc_rport *rport = transport_class_to_rport(dev);

	/* identify any roles that are port_id specific */
	if ((rport->port_id != -1) &&
	    (rport->port_id & FC_WELLKNOWN_PORTID_MASK) ==
					FC_WELLKNOWN_PORTID_MASK) {
		switch (rport->port_id & FC_WELLKNOWN_ROLE_MASK) {
		case FC_FPORT_PORTID:
			return snprintf(buf, 30, "Fabric Port\n");
		case FC_FABCTLR_PORTID:
			return snprintf(buf, 30, "Fabric Controller\n");
		case FC_DIRSRVR_PORTID:
			return snprintf(buf, 30, "Directory Server\n");
		case FC_TIMESRVR_PORTID:
			return snprintf(buf, 30, "Time Server\n");
		case FC_MGMTSRVR_PORTID:
			return snprintf(buf, 30, "Management Server\n");
		default:
			return snprintf(buf, 30, "Unknown Fabric Entity\n");
		}
	} else {
		if (rport->roles == FC_PORT_ROLE_UNKNOWN)
			return snprintf(buf, 20, "unknown\n");
		return get_fc_port_roles_names(rport->roles, buf);
	}
}
static FC_DEVICE_ATTR(rport, roles, S_IRUGO,
		show_fc_rport_roles, NULL);

fc_private_rport_rd_enum_attr(port_state, FC_PORTSTATE_MAX_NAMELEN);
fc_private_rport_rd_attr(scsi_target_id, "%d\n", 20);

/*
 * fast_io_fail_tmo attribute
 */
static ssize_t
show_fc_rport_fast_io_fail_tmo (struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct fc_rport *rport = transport_class_to_rport(dev);

	if (rport->fast_io_fail_tmo == -1)
		return snprintf(buf, 5, "off\n");
	return snprintf(buf, 20, "%d\n", rport->fast_io_fail_tmo);
}

static ssize_t
store_fc_rport_fast_io_fail_tmo(struct device *dev,
				struct device_attribute *attr, const char *buf,
				size_t count)
{
	int val;
	char *cp;
	struct fc_rport *rport = transport_class_to_rport(dev);

	if ((rport->port_state == FC_PORTSTATE_BLOCKED) ||
	    (rport->port_state == FC_PORTSTATE_DELETED) ||
	    (rport->port_state == FC_PORTSTATE_NOTPRESENT))
		return -EBUSY;
	if (strncmp(buf, "off", 3) == 0)
		rport->fast_io_fail_tmo = -1;
	else {
		val = simple_strtoul(buf, &cp, 0);
		if ((*cp && (*cp != '\n')) ||
		    (val < 0) || (val >= rport->dev_loss_tmo))
			return -EINVAL;
		rport->fast_io_fail_tmo = val;
	}
	return count;
}
static FC_DEVICE_ATTR(rport, fast_io_fail_tmo, S_IRUGO | S_IWUSR,
	show_fc_rport_fast_io_fail_tmo, store_fc_rport_fast_io_fail_tmo);


/*
 * FC SCSI Target Attribute Management
 */

/*
 * Note: in the target show function we recognize when the remote
 *  port is in the heirarchy and do not allow the driver to get
 *  involved in sysfs functions. The driver only gets involved if
 *  it's the "old" style that doesn't use rports.
 */
#define fc_starget_show_function(field, format_string, sz, cast)	\
static ssize_t								\
show_fc_starget_##field (struct device *dev, 				\
			 struct device_attribute *attr, char *buf)	\
{									\
	struct scsi_target *starget = transport_class_to_starget(dev);	\
	struct Scsi_Host *shost = dev_to_shost(starget->dev.parent);	\
	struct fc_internal *i = to_fc_internal(shost->transportt);	\
	struct fc_rport *rport = starget_to_rport(starget);		\
	if (rport)							\
		fc_starget_##field(starget) = rport->field;		\
	else if (i->f->get_starget_##field)				\
		i->f->get_starget_##field(starget);			\
	return snprintf(buf, sz, format_string, 			\
		cast fc_starget_##field(starget)); 			\
}

#define fc_starget_rd_attr(field, format_string, sz)			\
	fc_starget_show_function(field, format_string, sz, )		\
static FC_DEVICE_ATTR(starget, field, S_IRUGO,			\
			 show_fc_starget_##field, NULL)

#define fc_starget_rd_attr_cast(field, format_string, sz, cast)		\
	fc_starget_show_function(field, format_string, sz, (cast))	\
static FC_DEVICE_ATTR(starget, field, S_IRUGO,			\
			  show_fc_starget_##field, NULL)

#define SETUP_STARGET_ATTRIBUTE_RD(field)				\
	i->private_starget_attrs[count] = device_attr_starget_##field; \
	i->private_starget_attrs[count].attr.mode = S_IRUGO;		\
	i->private_starget_attrs[count].store = NULL;			\
	i->starget_attrs[count] = &i->private_starget_attrs[count];	\
	if (i->f->show_starget_##field)					\
		count++

#define SETUP_STARGET_ATTRIBUTE_RW(field)				\
	i->private_starget_attrs[count] = device_attr_starget_##field; \
	if (!i->f->set_starget_##field) {				\
		i->private_starget_attrs[count].attr.mode = S_IRUGO;	\
		i->private_starget_attrs[count].store = NULL;		\
	}								\
	i->starget_attrs[count] = &i->private_starget_attrs[count];	\
	if (i->f->show_starget_##field)					\
		count++

/* The FC Transport SCSI Target Attributes: */
fc_starget_rd_attr_cast(node_name, "0x%llx\n", 20, unsigned long long);
fc_starget_rd_attr_cast(port_name, "0x%llx\n", 20, unsigned long long);
fc_starget_rd_attr(port_id, "0x%06x\n", 20);


/*
 * FC Virtual Port Attribute Management
 */

#define fc_vport_show_function(field, format_string, sz, cast)		\
static ssize_t								\
show_fc_vport_##field (struct device *dev, 				\
		       struct device_attribute *attr, char *buf)	\
{									\
	struct fc_vport *vport = transport_class_to_vport(dev);		\
	struct Scsi_Host *shost = vport_to_shost(vport);		\
	struct fc_internal *i = to_fc_internal(shost->transportt);	\
	if ((i->f->get_vport_##field) &&				\
	    !(vport->flags & (FC_VPORT_DEL | FC_VPORT_CREATING)))	\
		i->f->get_vport_##field(vport);				\
	return snprintf(buf, sz, format_string, cast vport->field); 	\
}

#define fc_vport_store_function(field)					\
static ssize_t								\
store_fc_vport_##field(struct device *dev,				\
		       struct device_attribute *attr,			\
		       const char *buf,	size_t count)			\
{									\
	int val;							\
	struct fc_vport *vport = transport_class_to_vport(dev);		\
	struct Scsi_Host *shost = vport_to_shost(vport);		\
	struct fc_internal *i = to_fc_internal(shost->transportt);	\
	char *cp;							\
	if (vport->flags & (FC_VPORT_DEL | FC_VPORT_CREATING))	\
		return -EBUSY;						\
	val = simple_strtoul(buf, &cp, 0);				\
	if (*cp && (*cp != '\n'))					\
		return -EINVAL;						\
	i->f->set_vport_##field(vport, val);				\
	return count;							\
}

#define fc_vport_store_str_function(field, slen)			\
static ssize_t								\
store_fc_vport_##field(struct device *dev,				\
		       struct device_attribute *attr, 			\
		       const char *buf,	size_t count)			\
{									\
	struct fc_vport *vport = transport_class_to_vport(dev);		\
	struct Scsi_Host *shost = vport_to_shost(vport);		\
	struct fc_internal *i = to_fc_internal(shost->transportt);	\
	unsigned int cnt=count;						\
									\
	/* count may include a LF at end of string */			\
	if (buf[cnt-1] == '\n')						\
		cnt--;							\
	if (cnt > ((slen) - 1))						\
		return -EINVAL;						\
	memcpy(vport->field, buf, cnt);					\
	i->f->set_vport_##field(vport);					\
	return count;							\
}

#define fc_vport_rd_attr(field, format_string, sz)			\
	fc_vport_show_function(field, format_string, sz, )		\
static FC_DEVICE_ATTR(vport, field, S_IRUGO,			\
			 show_fc_vport_##field, NULL)

#define fc_vport_rd_attr_cast(field, format_string, sz, cast)		\
	fc_vport_show_function(field, format_string, sz, (cast))	\
static FC_DEVICE_ATTR(vport, field, S_IRUGO,			\
			  show_fc_vport_##field, NULL)

#define fc_vport_rw_attr(field, format_string, sz)			\
	fc_vport_show_function(field, format_string, sz, )		\
	fc_vport_store_function(field)					\
static FC_DEVICE_ATTR(vport, field, S_IRUGO | S_IWUSR,		\
			show_fc_vport_##field,				\
			store_fc_vport_##field)

#define fc_private_vport_show_function(field, format_string, sz, cast)	\
static ssize_t								\
show_fc_vport_##field (struct device *dev,				\
		       struct device_attribute *attr, char *buf)	\
{									\
	struct fc_vport *vport = transport_class_to_vport(dev);		\
	return snprintf(buf, sz, format_string, cast vport->field); 	\
}

#define fc_private_vport_store_u32_function(field)			\
static ssize_t								\
store_fc_vport_##field(struct device *dev,				\
		       struct device_attribute *attr,			\
		       const char *buf,	size_t count)			\
{									\
	u32 val;							\
	struct fc_vport *vport = transport_class_to_vport(dev);		\
	char *cp;							\
	if (vport->flags & (FC_VPORT_DEL | FC_VPORT_CREATING))		\
		return -EBUSY;						\
	val = simple_strtoul(buf, &cp, 0);				\
	if (*cp && (*cp != '\n'))					\
		return -EINVAL;						\
	vport->field = val;						\
	return count;							\
}


#define fc_private_vport_rd_attr(field, format_string, sz)		\
	fc_private_vport_show_function(field, format_string, sz, )	\
static FC_DEVICE_ATTR(vport, field, S_IRUGO,			\
			 show_fc_vport_##field, NULL)

#define fc_private_vport_rd_attr_cast(field, format_string, sz, cast)	\
	fc_private_vport_show_function(field, format_string, sz, (cast)) \
static FC_DEVICE_ATTR(vport, field, S_IRUGO,			\
			  show_fc_vport_##field, NULL)

#define fc_private_vport_rw_u32_attr(field, format_string, sz)		\
	fc_private_vport_show_function(field, format_string, sz, )	\
	fc_private_vport_store_u32_function(field)			\
static FC_DEVICE_ATTR(vport, field, S_IRUGO | S_IWUSR,		\
			show_fc_vport_##field,				\
			store_fc_vport_##field)


#define fc_private_vport_rd_enum_attr(title, maxlen)			\
static ssize_t								\
show_fc_vport_##title (struct device *dev,				\
		       struct device_attribute *attr,			\
		       char *buf)					\
{									\
	struct fc_vport *vport = transport_class_to_vport(dev);		\
	const char *name;						\
	name = get_fc_##title##_name(vport->title);			\
	if (!name)							\
		return -EINVAL;						\
	return snprintf(buf, maxlen, "%s\n", name);			\
}									\
static FC_DEVICE_ATTR(vport, title, S_IRUGO,			\
			show_fc_vport_##title, NULL)


#define SETUP_VPORT_ATTRIBUTE_RD(field)					\
	i->private_vport_attrs[count] = device_attr_vport_##field; \
	i->private_vport_attrs[count].attr.mode = S_IRUGO;		\
	i->private_vport_attrs[count].store = NULL;			\
	i->vport_attrs[count] = &i->private_vport_attrs[count];		\
	if (i->f->get_##field)						\
		count++
	/* NOTE: Above MACRO differs: checks function not show bit */

#define SETUP_PRIVATE_VPORT_ATTRIBUTE_RD(field)				\
	i->private_vport_attrs[count] = device_attr_vport_##field; \
	i->private_vport_attrs[count].attr.mode = S_IRUGO;		\
	i->private_vport_attrs[count].store = NULL;			\
	i->vport_attrs[count] = &i->private_vport_attrs[count];		\
	count++

#define SETUP_VPORT_ATTRIBUTE_WR(field)					\
	i->private_vport_attrs[count] = device_attr_vport_##field; \
	i->vport_attrs[count] = &i->private_vport_attrs[count];		\
	if (i->f->field)						\
		count++
	/* NOTE: Above MACRO differs: checks function */

#define SETUP_VPORT_ATTRIBUTE_RW(field)					\
	i->private_vport_attrs[count] = device_attr_vport_##field; \
	if (!i->f->set_vport_##field) {					\
		i->private_vport_attrs[count].attr.mode = S_IRUGO;	\
		i->private_vport_attrs[count].store = NULL;		\
	}								\
	i->vport_attrs[count] = &i->private_vport_attrs[count];		\
	count++
	/* NOTE: Above MACRO differs: does not check show bit */

#define SETUP_PRIVATE_VPORT_ATTRIBUTE_RW(field)				\
{									\
	i->private_vport_attrs[count] = device_attr_vport_##field; \
	i->vport_attrs[count] = &i->private_vport_attrs[count];		\
	count++;							\
}


/* The FC Transport Virtual Port Attributes: */

/* Fixed Virtual Port Attributes */

/* Dynamic Virtual Port Attributes */

/* Private Virtual Port Attributes */

fc_private_vport_rd_enum_attr(vport_state, FC_VPORTSTATE_MAX_NAMELEN);
fc_private_vport_rd_enum_attr(vport_last_state, FC_VPORTSTATE_MAX_NAMELEN);
fc_private_vport_rd_attr_cast(node_name, "0x%llx\n", 20, unsigned long long);
fc_private_vport_rd_attr_cast(port_name, "0x%llx\n", 20, unsigned long long);

static ssize_t
show_fc_vport_roles (struct device *dev, struct device_attribute *attr,
		     char *buf)
{
	struct fc_vport *vport = transport_class_to_vport(dev);

	if (vport->roles == FC_PORT_ROLE_UNKNOWN)
		return snprintf(buf, 20, "unknown\n");
	return get_fc_port_roles_names(vport->roles, buf);
}
static FC_DEVICE_ATTR(vport, roles, S_IRUGO, show_fc_vport_roles, NULL);

fc_private_vport_rd_enum_attr(vport_type, FC_PORTTYPE_MAX_NAMELEN);

fc_private_vport_show_function(symbolic_name, "%s\n",
		FC_VPORT_SYMBOLIC_NAMELEN + 1, )
fc_vport_store_str_function(symbolic_name, FC_VPORT_SYMBOLIC_NAMELEN)
static FC_DEVICE_ATTR(vport, symbolic_name, S_IRUGO | S_IWUSR,
		show_fc_vport_symbolic_name, store_fc_vport_symbolic_name);

static ssize_t
store_fc_vport_delete(struct device *dev, struct device_attribute *attr,
		      const char *buf, size_t count)
{
	struct fc_vport *vport = transport_class_to_vport(dev);
	struct Scsi_Host *shost = vport_to_shost(vport);

	fc_queue_work(shost, &vport->vport_delete_work);
	return count;
}
static FC_DEVICE_ATTR(vport, vport_delete, S_IWUSR,
			NULL, store_fc_vport_delete);


/*
 * Enable/Disable vport
 *  Write "1" to disable, write "0" to enable
 */
static ssize_t
store_fc_vport_disable(struct device *dev, struct device_attribute *attr,
		       const char *buf,
			   size_t count)
{
	struct fc_vport *vport = transport_class_to_vport(dev);
	struct Scsi_Host *shost = vport_to_shost(vport);
	struct fc_internal *i = to_fc_internal(shost->transportt);
	int stat;

	if (vport->flags & (FC_VPORT_DEL | FC_VPORT_CREATING))
		return -EBUSY;

	if (*buf == '0') {
		if (vport->vport_state != FC_VPORT_DISABLED)
			return -EALREADY;
	} else if (*buf == '1') {
		if (vport->vport_state == FC_VPORT_DISABLED)
			return -EALREADY;
	} else
		return -EINVAL;

	stat = i->f->vport_disable(vport, ((*buf == '0') ? false : true));
	return stat ? stat : count;
}
static FC_DEVICE_ATTR(vport, vport_disable, S_IWUSR,
			NULL, store_fc_vport_disable);


/*
 * Host Attribute Management
 */

#define fc_host_show_function(field, format_string, sz, cast)		\
static ssize_t								\
show_fc_host_##field (struct device *dev,				\
		      struct device_attribute *attr, char *buf)		\
{									\
	struct Scsi_Host *shost = transport_class_to_shost(dev);	\
	struct fc_internal *i = to_fc_internal(shost->transportt);	\
	if (i->f->get_host_##field)					\
		i->f->get_host_##field(shost);				\
	return snprintf(buf, sz, format_string, cast fc_host_##field(shost)); \
}

#define fc_host_store_function(field)					\
static ssize_t								\
store_fc_host_##field(struct device *dev, 				\
		      struct device_attribute *attr,			\
		      const char *buf,	size_t count)			\
{									\
	int val;							\
	struct Scsi_Host *shost = transport_class_to_shost(dev);	\
	struct fc_internal *i = to_fc_internal(shost->transportt);	\
	char *cp;							\
									\
	val = simple_strtoul(buf, &cp, 0);				\
	if (*cp && (*cp != '\n'))					\
		return -EINVAL;						\
	i->f->set_host_##field(shost, val);				\
	return count;							\
}

#define fc_host_store_str_function(field, slen)				\
static ssize_t								\
store_fc_host_##field(struct device *dev,				\
		      struct device_attribute *attr,			\
		      const char *buf, size_t count)			\
{									\
	struct Scsi_Host *shost = transport_class_to_shost(dev);	\
	struct fc_internal *i = to_fc_internal(shost->transportt);	\
	unsigned int cnt=count;						\
									\
	/* count may include a LF at end of string */			\
	if (buf[cnt-1] == '\n')						\
		cnt--;							\
	if (cnt > ((slen) - 1))						\
		return -EINVAL;						\
	memcpy(fc_host_##field(shost), buf, cnt);			\
	i->f->set_host_##field(shost);					\
	return count;							\
}

#define fc_host_rd_attr(field, format_string, sz)			\
	fc_host_show_function(field, format_string, sz, )		\
static FC_DEVICE_ATTR(host, field, S_IRUGO,			\
			 show_fc_host_##field, NULL)

#define fc_host_rd_attr_cast(field, format_string, sz, cast)		\
	fc_host_show_function(field, format_string, sz, (cast))		\
static FC_DEVICE_ATTR(host, field, S_IRUGO,			\
			  show_fc_host_##field, NULL)

#define fc_host_rw_attr(field, format_string, sz)			\
	fc_host_show_function(field, format_string, sz, )		\
	fc_host_store_function(field)					\
static FC_DEVICE_ATTR(host, field, S_IRUGO | S_IWUSR,		\
			show_fc_host_##field,				\
			store_fc_host_##field)

#define fc_host_rd_enum_attr(title, maxlen)				\
static ssize_t								\
show_fc_host_##title (struct device *dev,				\
		      struct device_attribute *attr, char *buf)		\
{									\
	struct Scsi_Host *shost = transport_class_to_shost(dev);	\
	struct fc_internal *i = to_fc_internal(shost->transportt);	\
	const char *name;						\
	if (i->f->get_host_##title)					\
		i->f->get_host_##title(shost);				\
	name = get_fc_##title##_name(fc_host_##title(shost));		\
	if (!name)							\
		return -EINVAL;						\
	return snprintf(buf, maxlen, "%s\n", name);			\
}									\
static FC_DEVICE_ATTR(host, title, S_IRUGO, show_fc_host_##title, NULL)

#define SETUP_HOST_ATTRIBUTE_RD(field)					\
	i->private_host_attrs[count] = device_attr_host_##field;	\
	i->private_host_attrs[count].attr.mode = S_IRUGO;		\
	i->private_host_attrs[count].store = NULL;			\
	i->host_attrs[count] = &i->private_host_attrs[count];		\
	if (i->f->show_host_##field)					\
		count++

#define SETUP_HOST_ATTRIBUTE_RD_NS(field)				\
	i->private_host_attrs[count] = device_attr_host_##field;	\
	i->private_host_attrs[count].attr.mode = S_IRUGO;		\
	i->private_host_attrs[count].store = NULL;			\
	i->host_attrs[count] = &i->private_host_attrs[count];		\
	count++

#define SETUP_HOST_ATTRIBUTE_RW(field)					\
	i->private_host_attrs[count] = device_attr_host_##field;	\
	if (!i->f->set_host_##field) {					\
		i->private_host_attrs[count].attr.mode = S_IRUGO;	\
		i->private_host_attrs[count].store = NULL;		\
	}								\
	i->host_attrs[count] = &i->private_host_attrs[count];		\
	if (i->f->show_host_##field)					\
		count++


#define fc_private_host_show_function(field, format_string, sz, cast)	\
static ssize_t								\
show_fc_host_##field (struct device *dev,				\
		      struct device_attribute *attr, char *buf)		\
{									\
	struct Scsi_Host *shost = transport_class_to_shost(dev);	\
	return snprintf(buf, sz, format_string, cast fc_host_##field(shost)); \
}

#define fc_private_host_rd_attr(field, format_string, sz)		\
	fc_private_host_show_function(field, format_string, sz, )	\
static FC_DEVICE_ATTR(host, field, S_IRUGO,			\
			 show_fc_host_##field, NULL)

#define fc_private_host_rd_attr_cast(field, format_string, sz, cast)	\
	fc_private_host_show_function(field, format_string, sz, (cast)) \
static FC_DEVICE_ATTR(host, field, S_IRUGO,			\
			  show_fc_host_##field, NULL)

#define SETUP_PRIVATE_HOST_ATTRIBUTE_RD(field)			\
	i->private_host_attrs[count] = device_attr_host_##field;	\
	i->private_host_attrs[count].attr.mode = S_IRUGO;		\
	i->private_host_attrs[count].store = NULL;			\
	i->host_attrs[count] = &i->private_host_attrs[count];		\
	count++

#define SETUP_PRIVATE_HOST_ATTRIBUTE_RW(field)			\
{									\
	i->private_host_attrs[count] = device_attr_host_##field;	\
	i->host_attrs[count] = &i->private_host_attrs[count];		\
	count++;							\
}


/* Fixed Host Attributes */

static ssize_t
show_fc_host_supported_classes (struct device *dev,
			        struct device_attribute *attr, char *buf)
{
	struct Scsi_Host *shost = transport_class_to_shost(dev);

	if (fc_host_supported_classes(shost) == FC_COS_UNSPECIFIED)
		return snprintf(buf, 20, "unspecified\n");

	return get_fc_cos_names(fc_host_supported_classes(shost), buf);
}
static FC_DEVICE_ATTR(host, supported_classes, S_IRUGO,
		show_fc_host_supported_classes, NULL);

static ssize_t
show_fc_host_supported_fc4s (struct device *dev,
			     struct device_attribute *attr, char *buf)
{
	struct Scsi_Host *shost = transport_class_to_shost(dev);
	return (ssize_t)show_fc_fc4s(buf, fc_host_supported_fc4s(shost));
}
static FC_DEVICE_ATTR(host, supported_fc4s, S_IRUGO,
		show_fc_host_supported_fc4s, NULL);

static ssize_t
show_fc_host_supported_speeds (struct device *dev,
			       struct device_attribute *attr, char *buf)
{
	struct Scsi_Host *shost = transport_class_to_shost(dev);

	if (fc_host_supported_speeds(shost) == FC_PORTSPEED_UNKNOWN)
		return snprintf(buf, 20, "unknown\n");

	return get_fc_port_speed_names(fc_host_supported_speeds(shost), buf);
}
static FC_DEVICE_ATTR(host, supported_speeds, S_IRUGO,
		show_fc_host_supported_speeds, NULL);


fc_private_host_rd_attr_cast(node_name, "0x%llx\n", 20, unsigned long long);
fc_private_host_rd_attr_cast(port_name, "0x%llx\n", 20, unsigned long long);
fc_private_host_rd_attr_cast(permanent_port_name, "0x%llx\n", 20,
			     unsigned long long);
fc_private_host_rd_attr(maxframe_size, "%u bytes\n", 20);
fc_private_host_rd_attr(max_npiv_vports, "%u\n", 20);
fc_private_host_rd_attr(serial_number, "%s\n", (FC_SERIAL_NUMBER_SIZE +1));


/* Dynamic Host Attributes */

static ssize_t
show_fc_host_active_fc4s (struct device *dev,
			  struct device_attribute *attr, char *buf)
{
	struct Scsi_Host *shost = transport_class_to_shost(dev);
	struct fc_internal *i = to_fc_internal(shost->transportt);

	if (i->f->get_host_active_fc4s)
		i->f->get_host_active_fc4s(shost);

	return (ssize_t)show_fc_fc4s(buf, fc_host_active_fc4s(shost));
}
static FC_DEVICE_ATTR(host, active_fc4s, S_IRUGO,
		show_fc_host_active_fc4s, NULL);

static ssize_t
show_fc_host_speed (struct device *dev,
		    struct device_attribute *attr, char *buf)
{
	struct Scsi_Host *shost = transport_class_to_shost(dev);
	struct fc_internal *i = to_fc_internal(shost->transportt);

	if (i->f->get_host_speed)
		i->f->get_host_speed(shost);

	if (fc_host_speed(shost) == FC_PORTSPEED_UNKNOWN)
		return snprintf(buf, 20, "unknown\n");

	return get_fc_port_speed_names(fc_host_speed(shost), buf);
}
static FC_DEVICE_ATTR(host, speed, S_IRUGO,
		show_fc_host_speed, NULL);


fc_host_rd_attr(port_id, "0x%06x\n", 20);
fc_host_rd_enum_attr(port_type, FC_PORTTYPE_MAX_NAMELEN);
fc_host_rd_enum_attr(port_state, FC_PORTSTATE_MAX_NAMELEN);
fc_host_rd_attr_cast(fabric_name, "0x%llx\n", 20, unsigned long long);
fc_host_rd_attr(symbolic_name, "%s\n", FC_SYMBOLIC_NAME_SIZE + 1);

fc_private_host_show_function(system_hostname, "%s\n",
		FC_SYMBOLIC_NAME_SIZE + 1, )
fc_host_store_str_function(system_hostname, FC_SYMBOLIC_NAME_SIZE)
static FC_DEVICE_ATTR(host, system_hostname, S_IRUGO | S_IWUSR,
		show_fc_host_system_hostname, store_fc_host_system_hostname);


/* Private Host Attributes */

static ssize_t
show_fc_private_host_tgtid_bind_type(struct device *dev,
				     struct device_attribute *attr, char *buf)
{
	struct Scsi_Host *shost = transport_class_to_shost(dev);
	const char *name;

	name = get_fc_tgtid_bind_type_name(fc_host_tgtid_bind_type(shost));
	if (!name)
		return -EINVAL;
	return snprintf(buf, FC_BINDTYPE_MAX_NAMELEN, "%s\n", name);
}

#define get_list_head_entry(pos, head, member) 		\
	pos = list_entry((head)->next, typeof(*pos), member)

static ssize_t
store_fc_private_host_tgtid_bind_type(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct Scsi_Host *shost = transport_class_to_shost(dev);
	struct fc_rport *rport;
 	enum fc_tgtid_binding_type val;
	unsigned long flags;

	if (get_fc_tgtid_bind_type_match(buf, &val))
		return -EINVAL;

	/* if changing bind type, purge all unused consistent bindings */
	if (val != fc_host_tgtid_bind_type(shost)) {
		spin_lock_irqsave(shost->host_lock, flags);
		while (!list_empty(&fc_host_rport_bindings(shost))) {
			get_list_head_entry(rport,
				&fc_host_rport_bindings(shost), peers);
			list_del(&rport->peers);
			rport->port_state = FC_PORTSTATE_DELETED;
			fc_queue_work(shost, &rport->rport_delete_work);
		}
		spin_unlock_irqrestore(shost->host_lock, flags);
	}

	fc_host_tgtid_bind_type(shost) = val;
	return count;
}

static FC_DEVICE_ATTR(host, tgtid_bind_type, S_IRUGO | S_IWUSR,
			show_fc_private_host_tgtid_bind_type,
			store_fc_private_host_tgtid_bind_type);

static ssize_t
store_fc_private_host_issue_lip(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct Scsi_Host *shost = transport_class_to_shost(dev);
	struct fc_internal *i = to_fc_internal(shost->transportt);
	int ret;

	/* ignore any data value written to the attribute */
	if (i->f->issue_fc_host_lip) {
		ret = i->f->issue_fc_host_lip(shost);
		return ret ? ret: count;
	}

	return -ENOENT;
}

static FC_DEVICE_ATTR(host, issue_lip, S_IWUSR, NULL,
			store_fc_private_host_issue_lip);

fc_private_host_rd_attr(npiv_vports_inuse, "%u\n", 20);


/*
 * Host Statistics Management
 */

/* Show a given an attribute in the statistics group */
static ssize_t
fc_stat_show(const struct device *dev, char *buf, unsigned long offset)
{
	struct Scsi_Host *shost = transport_class_to_shost(dev);
	struct fc_internal *i = to_fc_internal(shost->transportt);
	struct fc_host_statistics *stats;
	ssize_t ret = -ENOENT;

	if (offset > sizeof(struct fc_host_statistics) ||
	    offset % sizeof(u64) != 0)
		WARN_ON(1);

	if (i->f->get_fc_host_stats) {
		stats = (i->f->get_fc_host_stats)(shost);
		if (stats)
			ret = snprintf(buf, 20, "0x%llx\n",
			      (unsigned long long)*(u64 *)(((u8 *) stats) + offset));
	}
	return ret;
}


/* generate a read-only statistics attribute */
#define fc_host_statistic(name)						\
static ssize_t show_fcstat_##name(struct device *cd,			\
				  struct device_attribute *attr,	\
				  char *buf)				\
{									\
	return fc_stat_show(cd, buf, 					\
			    offsetof(struct fc_host_statistics, name));	\
}									\
static FC_DEVICE_ATTR(host, name, S_IRUGO, show_fcstat_##name, NULL)

fc_host_statistic(seconds_since_last_reset);
fc_host_statistic(tx_frames);
fc_host_statistic(tx_words);
fc_host_statistic(rx_frames);
fc_host_statistic(rx_words);
fc_host_statistic(lip_count);
fc_host_statistic(nos_count);
fc_host_statistic(error_frames);
fc_host_statistic(dumped_frames);
fc_host_statistic(link_failure_count);
fc_host_statistic(loss_of_sync_count);
fc_host_statistic(loss_of_signal_count);
fc_host_statistic(prim_seq_protocol_err_count);
fc_host_statistic(invalid_tx_word_count);
fc_host_statistic(invalid_crc_count);
fc_host_statistic(fcp_input_requests);
fc_host_statistic(fcp_output_requests);
fc_host_statistic(fcp_control_requests);
fc_host_statistic(fcp_input_megabytes);
fc_host_statistic(fcp_output_megabytes);

static ssize_t
fc_reset_statistics(struct device *dev, struct device_attribute *attr,
		    const char *buf, size_t count)
{
	struct Scsi_Host *shost = transport_class_to_shost(dev);
	struct fc_internal *i = to_fc_internal(shost->transportt);

	/* ignore any data value written to the attribute */
	if (i->f->reset_fc_host_stats) {
		i->f->reset_fc_host_stats(shost);
		return count;
	}

	return -ENOENT;
}
static FC_DEVICE_ATTR(host, reset_statistics, S_IWUSR, NULL,
				fc_reset_statistics);

static struct attribute *fc_statistics_attrs[] = {
	&device_attr_host_seconds_since_last_reset.attr,
	&device_attr_host_tx_frames.attr,
	&device_attr_host_tx_words.attr,
	&device_attr_host_rx_frames.attr,
	&device_attr_host_rx_words.attr,
	&device_attr_host_lip_count.attr,
	&device_attr_host_nos_count.attr,
	&device_attr_host_error_frames.attr,
	&device_attr_host_dumped_frames.attr,
	&device_attr_host_link_failure_count.attr,
	&device_attr_host_loss_of_sync_count.attr,
	&device_attr_host_loss_of_signal_count.attr,
	&device_attr_host_prim_seq_protocol_err_count.attr,
	&device_attr_host_invalid_tx_word_count.attr,
	&device_attr_host_invalid_crc_count.attr,
	&device_attr_host_fcp_input_requests.attr,
	&device_attr_host_fcp_output_requests.attr,
	&device_attr_host_fcp_control_requests.attr,
	&device_attr_host_fcp_input_megabytes.attr,
	&device_attr_host_fcp_output_megabytes.attr,
	&device_attr_host_reset_statistics.attr,
	NULL
};

static struct attribute_group fc_statistics_group = {
	.name = "statistics",
	.attrs = fc_statistics_attrs,
};


/* Host Vport Attributes */

static int
fc_parse_wwn(const char *ns, u64 *nm)
{
	unsigned int i, j;
	u8 wwn[8];

	memset(wwn, 0, sizeof(wwn));

	/* Validate and store the new name */
	for (i=0, j=0; i < 16; i++) {
		if ((*ns >= 'a') && (*ns <= 'f'))
			j = ((j << 4) | ((*ns++ -'a') + 10));
		else if ((*ns >= 'A') && (*ns <= 'F'))
			j = ((j << 4) | ((*ns++ -'A') + 10));
		else if ((*ns >= '0') && (*ns <= '9'))
			j = ((j << 4) | (*ns++ -'0'));
		else
			return -EINVAL;
		if (i % 2) {
			wwn[i/2] = j & 0xff;
			j = 0;
		}
	}

	*nm = wwn_to_u64(wwn);

	return 0;
}


/*
 * "Short-cut" sysfs variable to create a new vport on a FC Host.
 * Input is a string of the form "<WWPN>:<WWNN>". Other attributes
 * will default to a NPIV-based FCP_Initiator; The WWNs are specified
 * as hex characters, and may *not* contain any prefixes (e.g. 0x, x, etc)
 */
static ssize_t
store_fc_host_vport_create(struct device *dev, struct device_attribute *attr,
			   const char *buf, size_t count)
{
	struct Scsi_Host *shost = transport_class_to_shost(dev);
	struct fc_vport_identifiers vid;
	struct fc_vport *vport;
	unsigned int cnt=count;
	int stat;

	memset(&vid, 0, sizeof(vid));

	/* count may include a LF at end of string */
	if (buf[cnt-1] == '\n')
		cnt--;

	/* validate we have enough characters for WWPN */
	if ((cnt != (16+1+16)) || (buf[16] != ':'))
		return -EINVAL;

	stat = fc_parse_wwn(&buf[0], &vid.port_name);
	if (stat)
		return stat;

	stat = fc_parse_wwn(&buf[17], &vid.node_name);
	if (stat)
		return stat;

	vid.roles = FC_PORT_ROLE_FCP_INITIATOR;
	vid.vport_type = FC_PORTTYPE_NPIV;
	/* vid.symbolic_name is already zero/NULL's */
	vid.disable = false;		/* always enabled */

	/* we only allow support on Channel 0 !!! */
	stat = fc_vport_setup(shost, 0, &shost->shost_gendev, &vid, &vport);
	return stat ? stat : count;
}
static FC_DEVICE_ATTR(host, vport_create, S_IWUSR, NULL,
			store_fc_host_vport_create);


/*
 * "Short-cut" sysfs variable to delete a vport on a FC Host.
 * Vport is identified by a string containing "<WWPN>:<WWNN>".
 * The WWNs are specified as hex characters, and may *not* contain
 * any prefixes (e.g. 0x, x, etc)
 */
static ssize_t
store_fc_host_vport_delete(struct device *dev, struct device_attribute *attr,
			   const char *buf, size_t count)
{
	struct Scsi_Host *shost = transport_class_to_shost(dev);
	struct fc_host_attrs *fc_host = shost_to_fc_host(shost);
	struct fc_vport *vport;
	u64 wwpn, wwnn;
	unsigned long flags;
	unsigned int cnt=count;
	int stat, match;

	/* count may include a LF at end of string */
	if (buf[cnt-1] == '\n')
		cnt--;

	/* validate we have enough characters for WWPN */
	if ((cnt != (16+1+16)) || (buf[16] != ':'))
		return -EINVAL;

	stat = fc_parse_wwn(&buf[0], &wwpn);
	if (stat)
		return stat;

	stat = fc_parse_wwn(&buf[17], &wwnn);
	if (stat)
		return stat;

	spin_lock_irqsave(shost->host_lock, flags);
	match = 0;
	/* we only allow support on Channel 0 !!! */
	list_for_each_entry(vport, &fc_host->vports, peers) {
		if ((vport->channel == 0) &&
		    (vport->port_name == wwpn) && (vport->node_name == wwnn)) {
			match = 1;
			break;
		}
	}
	spin_unlock_irqrestore(shost->host_lock, flags);

	if (!match)
		return -ENODEV;

	stat = fc_vport_terminate(vport);
	return stat ? stat : count;
}
static FC_DEVICE_ATTR(host, vport_delete, S_IWUSR, NULL,
			store_fc_host_vport_delete);


static int fc_host_match(struct attribute_container *cont,
			  struct device *dev)
{
	struct Scsi_Host *shost;
	struct fc_internal *i;

	if (!scsi_is_host_device(dev))
		return 0;

	shost = dev_to_shost(dev);
	if (!shost->transportt  || shost->transportt->host_attrs.ac.class
	    != &fc_host_class.class)
		return 0;

	i = to_fc_internal(shost->transportt);

	return &i->t.host_attrs.ac == cont;
}

static int fc_target_match(struct attribute_container *cont,
			    struct device *dev)
{
	struct Scsi_Host *shost;
	struct fc_internal *i;

	if (!scsi_is_target_device(dev))
		return 0;

	shost = dev_to_shost(dev->parent);
	if (!shost->transportt  || shost->transportt->host_attrs.ac.class
	    != &fc_host_class.class)
		return 0;

	i = to_fc_internal(shost->transportt);

	return &i->t.target_attrs.ac == cont;
}

static void fc_rport_dev_release(struct device *dev)
{
	struct fc_rport *rport = dev_to_rport(dev);
	put_device(dev->parent);
	kfree(rport);
}

int scsi_is_fc_rport(const struct device *dev)
{
	return dev->release == fc_rport_dev_release;
}
EXPORT_SYMBOL(scsi_is_fc_rport);

static int fc_rport_match(struct attribute_container *cont,
			    struct device *dev)
{
	struct Scsi_Host *shost;
	struct fc_internal *i;

	if (!scsi_is_fc_rport(dev))
		return 0;

	shost = dev_to_shost(dev->parent);
	if (!shost->transportt  || shost->transportt->host_attrs.ac.class
	    != &fc_host_class.class)
		return 0;

	i = to_fc_internal(shost->transportt);

	return &i->rport_attr_cont.ac == cont;
}


static void fc_vport_dev_release(struct device *dev)
{
	struct fc_vport *vport = dev_to_vport(dev);
	put_device(dev->parent);		/* release kobj parent */
	kfree(vport);
}

int scsi_is_fc_vport(const struct device *dev)
{
	return dev->release == fc_vport_dev_release;
}
EXPORT_SYMBOL(scsi_is_fc_vport);

static int fc_vport_match(struct attribute_container *cont,
			    struct device *dev)
{
	struct fc_vport *vport;
	struct Scsi_Host *shost;
	struct fc_internal *i;

	if (!scsi_is_fc_vport(dev))
		return 0;
	vport = dev_to_vport(dev);

	shost = vport_to_shost(vport);
	if (!shost->transportt  || shost->transportt->host_attrs.ac.class
	    != &fc_host_class.class)
		return 0;

	i = to_fc_internal(shost->transportt);
	return &i->vport_attr_cont.ac == cont;
}


/**
 * fc_timed_out - FC Transport I/O timeout intercept handler
 * @scmd:	The SCSI command which timed out
 *
 * This routine protects against error handlers getting invoked while a
 * rport is in a blocked state, typically due to a temporarily loss of
 * connectivity. If the error handlers are allowed to proceed, requests
 * to abort i/o, reset the target, etc will likely fail as there is no way
 * to communicate with the device to perform the requested function. These
 * failures may result in the midlayer taking the device offline, requiring
 * manual intervention to restore operation.
 *
 * This routine, called whenever an i/o times out, validates the state of
 * the underlying rport. If the rport is blocked, it returns
 * EH_RESET_TIMER, which will continue to reschedule the timeout.
 * Eventually, either the device will return, or devloss_tmo will fire,
 * and when the timeout then fires, it will be handled normally.
 * If the rport is not blocked, normal error handling continues.
 *
 * Notes:
 *	This routine assumes no locks are held on entry.
 */
static enum blk_eh_timer_return
fc_timed_out(struct scsi_cmnd *scmd)
{
	struct fc_rport *rport = starget_to_rport(scsi_target(scmd->device));

	if (rport->port_state == FC_PORTSTATE_BLOCKED)
		return BLK_EH_RESET_TIMER;

	return BLK_EH_NOT_HANDLED;
}

/*
 * Called by fc_user_scan to locate an rport on the shost that
 * matches the channel and target id, and invoke scsi_scan_target()
 * on the rport.
 */
static void
fc_user_scan_tgt(struct Scsi_Host *shost, uint channel, uint id, uint lun)
{
	struct fc_rport *rport;
	unsigned long flags;

	spin_lock_irqsave(shost->host_lock, flags);

	list_for_each_entry(rport, &fc_host_rports(shost), peers) {
		if (rport->scsi_target_id == -1)
			continue;

		if (rport->port_state != FC_PORTSTATE_ONLINE)
			continue;

		if ((channel == rport->channel) &&
		    (id == rport->scsi_target_id)) {
			spin_unlock_irqrestore(shost->host_lock, flags);
			scsi_scan_target(&rport->dev, channel, id, lun, 1);
			return;
		}
	}

	spin_unlock_irqrestore(shost->host_lock, flags);
}

/*
 * Called via sysfs scan routines. Necessary, as the FC transport
 * wants to place all target objects below the rport object. So this
 * routine must invoke the scsi_scan_target() routine with the rport
 * object as the parent.
 */
static int
fc_user_scan(struct Scsi_Host *shost, uint channel, uint id, uint lun)
{
	uint chlo, chhi;
	uint tgtlo, tgthi;

	if (((channel != SCAN_WILD_CARD) && (channel > shost->max_channel)) ||
	    ((id != SCAN_WILD_CARD) && (id >= shost->max_id)) ||
	    ((lun != SCAN_WILD_CARD) && (lun > shost->max_lun)))
		return -EINVAL;

	if (channel == SCAN_WILD_CARD) {
		chlo = 0;
		chhi = shost->max_channel + 1;
	} else {
		chlo = channel;
		chhi = channel + 1;
	}

	if (id == SCAN_WILD_CARD) {
		tgtlo = 0;
		tgthi = shost->max_id;
	} else {
		tgtlo = id;
		tgthi = id + 1;
	}

	for ( ; chlo < chhi; chlo++)
		for ( ; tgtlo < tgthi; tgtlo++)
			fc_user_scan_tgt(shost, chlo, tgtlo, lun);

	return 0;
}

static int fc_tsk_mgmt_response(struct Scsi_Host *shost, u64 nexus, u64 tm_id,
				int result)
{
	struct fc_internal *i = to_fc_internal(shost->transportt);
	return i->f->tsk_mgmt_response(shost, nexus, tm_id, result);
}

static int fc_it_nexus_response(struct Scsi_Host *shost, u64 nexus, int result)
{
	struct fc_internal *i = to_fc_internal(shost->transportt);
	return i->f->it_nexus_response(shost, nexus, result);
}

struct scsi_transport_template *
fc_attach_transport(struct fc_function_template *ft)
{
	int count;
	struct fc_internal *i = kzalloc(sizeof(struct fc_internal),
					GFP_KERNEL);

	if (unlikely(!i))
		return NULL;

	i->t.target_attrs.ac.attrs = &i->starget_attrs[0];
	i->t.target_attrs.ac.class = &fc_transport_class.class;
	i->t.target_attrs.ac.match = fc_target_match;
	i->t.target_size = sizeof(struct fc_starget_attrs);
	transport_container_register(&i->t.target_attrs);

	i->t.host_attrs.ac.attrs = &i->host_attrs[0];
	i->t.host_attrs.ac.class = &fc_host_class.class;
	i->t.host_attrs.ac.match = fc_host_match;
	i->t.host_size = sizeof(struct fc_host_attrs);
	if (ft->get_fc_host_stats)
		i->t.host_attrs.statistics = &fc_statistics_group;
	transport_container_register(&i->t.host_attrs);

	i->rport_attr_cont.ac.attrs = &i->rport_attrs[0];
	i->rport_attr_cont.ac.class = &fc_rport_class.class;
	i->rport_attr_cont.ac.match = fc_rport_match;
	transport_container_register(&i->rport_attr_cont);

	i->vport_attr_cont.ac.attrs = &i->vport_attrs[0];
	i->vport_attr_cont.ac.class = &fc_vport_class.class;
	i->vport_attr_cont.ac.match = fc_vport_match;
	transport_container_register(&i->vport_attr_cont);

	i->f = ft;

	/* Transport uses the shost workq for scsi scanning */
	i->t.create_work_queue = 1;

	i->t.eh_timed_out = fc_timed_out;

	i->t.user_scan = fc_user_scan;

	/* target-mode drivers' functions */
	i->t.tsk_mgmt_response = fc_tsk_mgmt_response;
	i->t.it_nexus_response = fc_it_nexus_response;

	/*
	 * Setup SCSI Target Attributes.
	 */
	count = 0;
	SETUP_STARGET_ATTRIBUTE_RD(node_name);
	SETUP_STARGET_ATTRIBUTE_RD(port_name);
	SETUP_STARGET_ATTRIBUTE_RD(port_id);

	BUG_ON(count > FC_STARGET_NUM_ATTRS);

	i->starget_attrs[count] = NULL;


	/*
	 * Setup SCSI Host Attributes.
	 */
	count=0;
	SETUP_HOST_ATTRIBUTE_RD(node_name);
	SETUP_HOST_ATTRIBUTE_RD(port_name);
	SETUP_HOST_ATTRIBUTE_RD(permanent_port_name);
	SETUP_HOST_ATTRIBUTE_RD(supported_classes);
	SETUP_HOST_ATTRIBUTE_RD(supported_fc4s);
	SETUP_HOST_ATTRIBUTE_RD(supported_speeds);
	SETUP_HOST_ATTRIBUTE_RD(maxframe_size);
	if (ft->vport_create) {
		SETUP_HOST_ATTRIBUTE_RD_NS(max_npiv_vports);
		SETUP_HOST_ATTRIBUTE_RD_NS(npiv_vports_inuse);
	}
	SETUP_HOST_ATTRIBUTE_RD(serial_number);

	SETUP_HOST_ATTRIBUTE_RD(port_id);
	SETUP_HOST_ATTRIBUTE_RD(port_type);
	SETUP_HOST_ATTRIBUTE_RD(port_state);
	SETUP_HOST_ATTRIBUTE_RD(active_fc4s);
	SETUP_HOST_ATTRIBUTE_RD(speed);
	SETUP_HOST_ATTRIBUTE_RD(fabric_name);
	SETUP_HOST_ATTRIBUTE_RD(symbolic_name);
	SETUP_HOST_ATTRIBUTE_RW(system_hostname);

	/* Transport-managed attributes */
	SETUP_PRIVATE_HOST_ATTRIBUTE_RW(tgtid_bind_type);
	if (ft->issue_fc_host_lip)
		SETUP_PRIVATE_HOST_ATTRIBUTE_RW(issue_lip);
	if (ft->vport_create)
		SETUP_PRIVATE_HOST_ATTRIBUTE_RW(vport_create);
	if (ft->vport_delete)
		SETUP_PRIVATE_HOST_ATTRIBUTE_RW(vport_delete);

	BUG_ON(count > FC_HOST_NUM_ATTRS);

	i->host_attrs[count] = NULL;

	/*
	 * Setup Remote Port Attributes.
	 */
	count=0;
	SETUP_RPORT_ATTRIBUTE_RD(maxframe_size);
	SETUP_RPORT_ATTRIBUTE_RD(supported_classes);
	SETUP_RPORT_ATTRIBUTE_RW(dev_loss_tmo);
	SETUP_PRIVATE_RPORT_ATTRIBUTE_RD(node_name);
	SETUP_PRIVATE_RPORT_ATTRIBUTE_RD(port_name);
	SETUP_PRIVATE_RPORT_ATTRIBUTE_RD(port_id);
	SETUP_PRIVATE_RPORT_ATTRIBUTE_RD(roles);
	SETUP_PRIVATE_RPORT_ATTRIBUTE_RD(port_state);
	SETUP_PRIVATE_RPORT_ATTRIBUTE_RD(scsi_target_id);
	SETUP_PRIVATE_RPORT_ATTRIBUTE_RW(fast_io_fail_tmo);

	BUG_ON(count > FC_RPORT_NUM_ATTRS);

	i->rport_attrs[count] = NULL;

	/*
	 * Setup Virtual Port Attributes.
	 */
	count=0;
	SETUP_PRIVATE_VPORT_ATTRIBUTE_RD(vport_state);
	SETUP_PRIVATE_VPORT_ATTRIBUTE_RD(vport_last_state);
	SETUP_PRIVATE_VPORT_ATTRIBUTE_RD(node_name);
	SETUP_PRIVATE_VPORT_ATTRIBUTE_RD(port_name);
	SETUP_PRIVATE_VPORT_ATTRIBUTE_RD(roles);
	SETUP_PRIVATE_VPORT_ATTRIBUTE_RD(vport_type);
	SETUP_VPORT_ATTRIBUTE_RW(symbolic_name);
	SETUP_VPORT_ATTRIBUTE_WR(vport_delete);
	SETUP_VPORT_ATTRIBUTE_WR(vport_disable);

	BUG_ON(count > FC_VPORT_NUM_ATTRS);

	i->vport_attrs[count] = NULL;

	return &i->t;
}
EXPORT_SYMBOL(fc_attach_transport);

void fc_release_transport(struct scsi_transport_template *t)
{
	struct fc_internal *i = to_fc_internal(t);

	transport_container_unregister(&i->t.target_attrs);
	transport_container_unregister(&i->t.host_attrs);
	transport_container_unregister(&i->rport_attr_cont);
	transport_container_unregister(&i->vport_attr_cont);

	kfree(i);
}
EXPORT_SYMBOL(fc_release_transport);

/**
 * fc_queue_work - Queue work to the fc_host workqueue.
 * @shost:	Pointer to Scsi_Host bound to fc_host.
 * @work:	Work to queue for execution.
 *
 * Return value:
 * 	1 - work queued for execution
 *	0 - work is already queued
 *	-EINVAL - work queue doesn't exist
 */
static int
fc_queue_work(struct Scsi_Host *shost, struct work_struct *work)
{
	if (unlikely(!fc_host_work_q(shost))) {
		printk(KERN_ERR
			"ERROR: FC host '%s' attempted to queue work, "
			"when no workqueue created.\n", shost->hostt->name);
		dump_stack();

		return -EINVAL;
	}

	return queue_work(fc_host_work_q(shost), work);
}

/**
 * fc_flush_work - Flush a fc_host's workqueue.
 * @shost:	Pointer to Scsi_Host bound to fc_host.
 */
static void
fc_flush_work(struct Scsi_Host *shost)
{
	if (!fc_host_work_q(shost)) {
		printk(KERN_ERR
			"ERROR: FC host '%s' attempted to flush work, "
			"when no workqueue created.\n", shost->hostt->name);
		dump_stack();
		return;
	}

	flush_workqueue(fc_host_work_q(shost));
}

/**
 * fc_queue_devloss_work - Schedule work for the fc_host devloss workqueue.
 * @shost:	Pointer to Scsi_Host bound to fc_host.
 * @work:	Work to queue for execution.
 * @delay:	jiffies to delay the work queuing
 *
 * Return value:
 * 	1 on success / 0 already queued / < 0 for error
 */
static int
fc_queue_devloss_work(struct Scsi_Host *shost, struct delayed_work *work,
				unsigned long delay)
{
	if (unlikely(!fc_host_devloss_work_q(shost))) {
		printk(KERN_ERR
			"ERROR: FC host '%s' attempted to queue work, "
			"when no workqueue created.\n", shost->hostt->name);
		dump_stack();

		return -EINVAL;
	}

	return queue_delayed_work(fc_host_devloss_work_q(shost), work, delay);
}

/**
 * fc_flush_devloss - Flush a fc_host's devloss workqueue.
 * @shost:	Pointer to Scsi_Host bound to fc_host.
 */
static void
fc_flush_devloss(struct Scsi_Host *shost)
{
	if (!fc_host_devloss_work_q(shost)) {
		printk(KERN_ERR
			"ERROR: FC host '%s' attempted to flush work, "
			"when no workqueue created.\n", shost->hostt->name);
		dump_stack();
		return;
	}

	flush_workqueue(fc_host_devloss_work_q(shost));
}


/**
 * fc_remove_host - called to terminate any fc_transport-related elements for a scsi host.
 * @shost:	Which &Scsi_Host
 *
 * This routine is expected to be called immediately preceeding the
 * a driver's call to scsi_remove_host().
 *
 * WARNING: A driver utilizing the fc_transport, which fails to call
 *   this routine prior to scsi_remove_host(), will leave dangling
 *   objects in /sys/class/fc_remote_ports. Access to any of these
 *   objects can result in a system crash !!!
 *
 * Notes:
 *	This routine assumes no locks are held on entry.
 */
void
fc_remove_host(struct Scsi_Host *shost)
{
	struct fc_vport *vport = NULL, *next_vport = NULL;
	struct fc_rport *rport = NULL, *next_rport = NULL;
	struct workqueue_struct *work_q;
	struct fc_host_attrs *fc_host = shost_to_fc_host(shost);
	unsigned long flags;

	spin_lock_irqsave(shost->host_lock, flags);

	/* Remove any vports */
	list_for_each_entry_safe(vport, next_vport, &fc_host->vports, peers)
		fc_queue_work(shost, &vport->vport_delete_work);

	/* Remove any remote ports */
	list_for_each_entry_safe(rport, next_rport,
			&fc_host->rports, peers) {
		list_del(&rport->peers);
		rport->port_state = FC_PORTSTATE_DELETED;
		fc_queue_work(shost, &rport->rport_delete_work);
	}

	list_for_each_entry_safe(rport, next_rport,
			&fc_host->rport_bindings, peers) {
		list_del(&rport->peers);
		rport->port_state = FC_PORTSTATE_DELETED;
		fc_queue_work(shost, &rport->rport_delete_work);
	}

	spin_unlock_irqrestore(shost->host_lock, flags);

	/* flush all scan work items */
	scsi_flush_work(shost);

	/* flush all stgt delete, and rport delete work items, then kill it  */
	if (fc_host->work_q) {
		work_q = fc_host->work_q;
		fc_host->work_q = NULL;
		destroy_workqueue(work_q);
	}

	/* flush all devloss work items, then kill it  */
	if (fc_host->devloss_work_q) {
		work_q = fc_host->devloss_work_q;
		fc_host->devloss_work_q = NULL;
		destroy_workqueue(work_q);
	}
}
EXPORT_SYMBOL(fc_remove_host);

static void fc_terminate_rport_io(struct fc_rport *rport)
{
	struct Scsi_Host *shost = rport_to_shost(rport);
	struct fc_internal *i = to_fc_internal(shost->transportt);

	/* Involve the LLDD if possible to terminate all io on the rport. */
	if (i->f->terminate_rport_io)
		i->f->terminate_rport_io(rport);

	/*
	 * must unblock to flush queued IO. The caller will have set
	 * the port_state or flags, so that fc_remote_port_chkready will
	 * fail IO.
	 */
	scsi_target_unblock(&rport->dev);
}

/**
 * fc_starget_delete - called to delete the scsi decendents of an rport
 * @work:	remote port to be operated on.
 *
 * Deletes target and all sdevs.
 */
static void
fc_starget_delete(struct work_struct *work)
{
	struct fc_rport *rport =
		container_of(work, struct fc_rport, stgt_delete_work);

	fc_terminate_rport_io(rport);
	scsi_remove_target(&rport->dev);
}


/**
 * fc_rport_final_delete - finish rport termination and delete it.
 * @work:	remote port to be deleted.
 */
static void
fc_rport_final_delete(struct work_struct *work)
{
	struct fc_rport *rport =
		container_of(work, struct fc_rport, rport_delete_work);
	struct device *dev = &rport->dev;
	struct Scsi_Host *shost = rport_to_shost(rport);
	struct fc_internal *i = to_fc_internal(shost->transportt);
	unsigned long flags;

	/*
	 * if a scan is pending, flush the SCSI Host work_q so that
	 * that we can reclaim the rport scan work element.
	 */
	if (rport->flags & FC_RPORT_SCAN_PENDING)
		scsi_flush_work(shost);

	fc_terminate_rport_io(rport);

	/*
	 * Cancel any outstanding timers. These should really exist
	 * only when rmmod'ing the LLDD and we're asking for
	 * immediate termination of the rports
	 */
	spin_lock_irqsave(shost->host_lock, flags);
	if (rport->flags & FC_RPORT_DEVLOSS_PENDING) {
		spin_unlock_irqrestore(shost->host_lock, flags);
		if (!cancel_delayed_work(&rport->fail_io_work))
			fc_flush_devloss(shost);
		if (!cancel_delayed_work(&rport->dev_loss_work))
			fc_flush_devloss(shost);
		spin_lock_irqsave(shost->host_lock, flags);
		rport->flags &= ~FC_RPORT_DEVLOSS_PENDING;
	}
	spin_unlock_irqrestore(shost->host_lock, flags);

	/* Delete SCSI target and sdevs */
	if (rport->scsi_target_id != -1)
		fc_starget_delete(&rport->stgt_delete_work);

	/*
	 * Notify the driver that the rport is now dead. The LLDD will
	 * also guarantee that any communication to the rport is terminated
	 *
	 * Avoid this call if we already called it when we preserved the
	 * rport for the binding.
	 */
	if (!(rport->flags & FC_RPORT_DEVLOSS_CALLBK_DONE) &&
	    (i->f->dev_loss_tmo_callbk))
		i->f->dev_loss_tmo_callbk(rport);

	fc_bsg_remove(rport->rqst_q);

	transport_remove_device(dev);
	device_del(dev);
	transport_destroy_device(dev);
	put_device(&shost->shost_gendev);	/* for fc_host->rport list */
	put_device(dev);			/* for self-reference */
}


/**
 * fc_rport_create - allocates and creates a remote FC port.
 * @shost:	scsi host the remote port is connected to.
 * @channel:	Channel on shost port connected to.
 * @ids:	The world wide names, fc address, and FC4 port
 *		roles for the remote port.
 *
 * Allocates and creates the remoter port structure, including the
 * class and sysfs creation.
 *
 * Notes:
 *	This routine assumes no locks are held on entry.
 */
static struct fc_rport *
fc_rport_create(struct Scsi_Host *shost, int channel,
	struct fc_rport_identifiers  *ids)
{
	struct fc_host_attrs *fc_host = shost_to_fc_host(shost);
	struct fc_internal *fci = to_fc_internal(shost->transportt);
	struct fc_rport *rport;
	struct device *dev;
	unsigned long flags;
	int error;
	size_t size;

	size = (sizeof(struct fc_rport) + fci->f->dd_fcrport_size);
	rport = kzalloc(size, GFP_KERNEL);
	if (unlikely(!rport)) {
		printk(KERN_ERR "%s: allocation failure\n", __func__);
		return NULL;
	}

	rport->maxframe_size = -1;
	rport->supported_classes = FC_COS_UNSPECIFIED;
	rport->dev_loss_tmo = fc_dev_loss_tmo;
	memcpy(&rport->node_name, &ids->node_name, sizeof(rport->node_name));
	memcpy(&rport->port_name, &ids->port_name, sizeof(rport->port_name));
	rport->port_id = ids->port_id;
	rport->roles = ids->roles;
	rport->port_state = FC_PORTSTATE_ONLINE;
	if (fci->f->dd_fcrport_size)
		rport->dd_data = &rport[1];
	rport->channel = channel;
	rport->fast_io_fail_tmo = -1;

	INIT_DELAYED_WORK(&rport->dev_loss_work, fc_timeout_deleted_rport);
	INIT_DELAYED_WORK(&rport->fail_io_work, fc_timeout_fail_rport_io);
	INIT_WORK(&rport->scan_work, fc_scsi_scan_rport);
	INIT_WORK(&rport->stgt_delete_work, fc_starget_delete);
	INIT_WORK(&rport->rport_delete_work, fc_rport_final_delete);

	spin_lock_irqsave(shost->host_lock, flags);

	rport->number = fc_host->next_rport_number++;
	if (rport->roles & FC_PORT_ROLE_FCP_TARGET)
		rport->scsi_target_id = fc_host->next_target_id++;
	else
		rport->scsi_target_id = -1;
	list_add_tail(&rport->peers, &fc_host->rports);
	get_device(&shost->shost_gendev);	/* for fc_host->rport list */

	spin_unlock_irqrestore(shost->host_lock, flags);

	dev = &rport->dev;
	device_initialize(dev);			/* takes self reference */
	dev->parent = get_device(&shost->shost_gendev); /* parent reference */
	dev->release = fc_rport_dev_release;
	dev_set_name(dev, "rport-%d:%d-%d",
		     shost->host_no, channel, rport->number);
	transport_setup_device(dev);

	error = device_add(dev);
	if (error) {
		printk(KERN_ERR "FC Remote Port device_add failed\n");
		goto delete_rport;
	}
	transport_add_device(dev);
	transport_configure_device(dev);

	fc_bsg_rportadd(shost, rport);
	/* ignore any bsg add error - we just can't do sgio */

	if (rport->roles & FC_PORT_ROLE_FCP_TARGET) {
		/* initiate a scan of the target */
		rport->flags |= FC_RPORT_SCAN_PENDING;
		scsi_queue_work(shost, &rport->scan_work);
	}

	return rport;

delete_rport:
	transport_destroy_device(dev);
	spin_lock_irqsave(shost->host_lock, flags);
	list_del(&rport->peers);
	put_device(&shost->shost_gendev);	/* for fc_host->rport list */
	spin_unlock_irqrestore(shost->host_lock, flags);
	put_device(dev->parent);
	kfree(rport);
	return NULL;
}

/**
 * fc_remote_port_add - notify fc transport of the existence of a remote FC port.
 * @shost:	scsi host the remote port is connected to.
 * @channel:	Channel on shost port connected to.
 * @ids:	The world wide names, fc address, and FC4 port
 *		roles for the remote port.
 *
 * The LLDD calls this routine to notify the transport of the existence
 * of a remote port. The LLDD provides the unique identifiers (wwpn,wwn)
 * of the port, it's FC address (port_id), and the FC4 roles that are
 * active for the port.
 *
 * For ports that are FCP targets (aka scsi targets), the FC transport
 * maintains consistent target id bindings on behalf of the LLDD.
 * A consistent target id binding is an assignment of a target id to
 * a remote port identifier, which persists while the scsi host is
 * attached. The remote port can disappear, then later reappear, and
 * it's target id assignment remains the same. This allows for shifts
 * in FC addressing (if binding by wwpn or wwnn) with no apparent
 * changes to the scsi subsystem which is based on scsi host number and
 * target id values.  Bindings are only valid during the attachment of
 * the scsi host. If the host detaches, then later re-attaches, target
 * id bindings may change.
 *
 * This routine is responsible for returning a remote port structure.
 * The routine will search the list of remote ports it maintains
 * internally on behalf of consistent target id mappings. If found, the
 * remote port structure will be reused. Otherwise, a new remote port
 * structure will be allocated.
 *
 * Whenever a remote port is allocated, a new fc_remote_port class
 * device is created.
 *
 * Should not be called from interrupt context.
 *
 * Notes:
 *	This routine assumes no locks are held on entry.
 */
struct fc_rport *
fc_remote_port_add(struct Scsi_Host *shost, int channel,
	struct fc_rport_identifiers  *ids)
{
	struct fc_internal *fci = to_fc_internal(shost->transportt);
	struct fc_host_attrs *fc_host = shost_to_fc_host(shost);
	struct fc_rport *rport;
	unsigned long flags;
	int match = 0;

	/* ensure any stgt delete functions are done */
	fc_flush_work(shost);

	/*
	 * Search the list of "active" rports, for an rport that has been
	 * deleted, but we've held off the real delete while the target
	 * is in a "blocked" state.
	 */
	spin_lock_irqsave(shost->host_lock, flags);

	list_for_each_entry(rport, &fc_host->rports, peers) {

		if ((rport->port_state == FC_PORTSTATE_BLOCKED) &&
			(rport->channel == channel)) {

			switch (fc_host->tgtid_bind_type) {
			case FC_TGTID_BIND_BY_WWPN:
			case FC_TGTID_BIND_NONE:
				if (rport->port_name == ids->port_name)
					match = 1;
				break;
			case FC_TGTID_BIND_BY_WWNN:
				if (rport->node_name == ids->node_name)
					match = 1;
				break;
			case FC_TGTID_BIND_BY_ID:
				if (rport->port_id == ids->port_id)
					match = 1;
				break;
			}

			if (match) {

				memcpy(&rport->node_name, &ids->node_name,
					sizeof(rport->node_name));
				memcpy(&rport->port_name, &ids->port_name,
					sizeof(rport->port_name));
				rport->port_id = ids->port_id;

				rport->port_state = FC_PORTSTATE_ONLINE;
				rport->roles = ids->roles;

				spin_unlock_irqrestore(shost->host_lock, flags);

				if (fci->f->dd_fcrport_size)
					memset(rport->dd_data, 0,
						fci->f->dd_fcrport_size);

				/*
				 * If we were not a target, cancel the
				 * io terminate and rport timers, and
				 * we're done.
				 *
				 * If we were a target, but our new role
				 * doesn't indicate a target, leave the
				 * timers running expecting the role to
				 * change as the target fully logs in. If
				 * it doesn't, the target will be torn down.
				 *
				 * If we were a target, and our role shows
				 * we're still a target, cancel the timers
				 * and kick off a scan.
				 */

				/* was a target, not in roles */
				if ((rport->scsi_target_id != -1) &&
				    (!(ids->roles & FC_PORT_ROLE_FCP_TARGET)))
					return rport;

				/*
				 * Stop the fail io and dev_loss timers.
				 * If they flush, the port_state will
				 * be checked and will NOOP the function.
				 */
				if (!cancel_delayed_work(&rport->fail_io_work))
					fc_flush_devloss(shost);
				if (!cancel_delayed_work(&rport->dev_loss_work))
					fc_flush_devloss(shost);

				spin_lock_irqsave(shost->host_lock, flags);

				rport->flags &= ~(FC_RPORT_FAST_FAIL_TIMEDOUT |
						  FC_RPORT_DEVLOSS_PENDING |
						  FC_RPORT_DEVLOSS_CALLBK_DONE);

				/* if target, initiate a scan */
				if (rport->scsi_target_id != -1) {
					rport->flags |= FC_RPORT_SCAN_PENDING;
					scsi_queue_work(shost,
							&rport->scan_work);
					spin_unlock_irqrestore(shost->host_lock,
							flags);
					scsi_target_unblock(&rport->dev);
				} else
					spin_unlock_irqrestore(shost->host_lock,
							flags);

				fc_bsg_goose_queue(rport);

				return rport;
			}
		}
	}

	/*
	 * Search the bindings array
	 * Note: if never a FCP target, you won't be on this list
	 */
	if (fc_host->tgtid_bind_type != FC_TGTID_BIND_NONE) {

		/* search for a matching consistent binding */

		list_for_each_entry(rport, &fc_host->rport_bindings,
					peers) {
			if (rport->channel != channel)
				continue;

			switch (fc_host->tgtid_bind_type) {
			case FC_TGTID_BIND_BY_WWPN:
				if (rport->port_name == ids->port_name)
					match = 1;
				break;
			case FC_TGTID_BIND_BY_WWNN:
				if (rport->node_name == ids->node_name)
					match = 1;
				break;
			case FC_TGTID_BIND_BY_ID:
				if (rport->port_id == ids->port_id)
					match = 1;
				break;
			case FC_TGTID_BIND_NONE: /* to keep compiler happy */
				break;
			}

			if (match) {
				list_move_tail(&rport->peers, &fc_host->rports);
				break;
			}
		}

		if (match) {
			memcpy(&rport->node_name, &ids->node_name,
				sizeof(rport->node_name));
			memcpy(&rport->port_name, &ids->port_name,
				sizeof(rport->port_name));
			rport->port_id = ids->port_id;
			rport->roles = ids->roles;
			rport->port_state = FC_PORTSTATE_ONLINE;
			rport->flags &= ~FC_RPORT_FAST_FAIL_TIMEDOUT;

			if (fci->f->dd_fcrport_size)
				memset(rport->dd_data, 0,
						fci->f->dd_fcrport_size);

			if (rport->roles & FC_PORT_ROLE_FCP_TARGET) {
				/* initiate a scan of the target */
				rport->flags |= FC_RPORT_SCAN_PENDING;
				scsi_queue_work(shost, &rport->scan_work);
				spin_unlock_irqrestore(shost->host_lock, flags);
				scsi_target_unblock(&rport->dev);
			} else
				spin_unlock_irqrestore(shost->host_lock, flags);

			return rport;
		}
	}

	spin_unlock_irqrestore(shost->host_lock, flags);

	/* No consistent binding found - create new remote port entry */
	rport = fc_rport_create(shost, channel, ids);

	return rport;
}
EXPORT_SYMBOL(fc_remote_port_add);


/**
 * fc_remote_port_delete - notifies the fc transport that a remote port is no longer in existence.
 * @rport:	The remote port that no longer exists
 *
 * The LLDD calls this routine to notify the transport that a remote
 * port is no longer part of the topology. Note: Although a port
 * may no longer be part of the topology, it may persist in the remote
 * ports displayed by the fc_host. We do this under 2 conditions:
 * 1) If the port was a scsi target, we delay its deletion by "blocking" it.
 *   This allows the port to temporarily disappear, then reappear without
 *   disrupting the SCSI device tree attached to it. During the "blocked"
 *   period the port will still exist.
 * 2) If the port was a scsi target and disappears for longer than we
 *   expect, we'll delete the port and the tear down the SCSI device tree
 *   attached to it. However, we want to semi-persist the target id assigned
 *   to that port if it eventually does exist. The port structure will
 *   remain (although with minimal information) so that the target id
 *   bindings remails.
 *
 * If the remote port is not an FCP Target, it will be fully torn down
 * and deallocated, including the fc_remote_port class device.
 *
 * If the remote port is an FCP Target, the port will be placed in a
 * temporary blocked state. From the LLDD's perspective, the rport no
 * longer exists. From the SCSI midlayer's perspective, the SCSI target
 * exists, but all sdevs on it are blocked from further I/O. The following
 * is then expected.
 *
 *   If the remote port does not return (signaled by a LLDD call to
 *   fc_remote_port_add()) within the dev_loss_tmo timeout, then the
 *   scsi target is removed - killing all outstanding i/o and removing the
 *   scsi devices attached ot it. The port structure will be marked Not
 *   Present and be partially cleared, leaving only enough information to
 *   recognize the remote port relative to the scsi target id binding if
 *   it later appears.  The port will remain as long as there is a valid
 *   binding (e.g. until the user changes the binding type or unloads the
 *   scsi host with the binding).
 *
 *   If the remote port returns within the dev_loss_tmo value (and matches
 *   according to the target id binding type), the port structure will be
 *   reused. If it is no longer a SCSI target, the target will be torn
 *   down. If it continues to be a SCSI target, then the target will be
 *   unblocked (allowing i/o to be resumed), and a scan will be activated
 *   to ensure that all luns are detected.
 *
 * Called from normal process context only - cannot be called from interrupt.
 *
 * Notes:
 *	This routine assumes no locks are held on entry.
 */
void
fc_remote_port_delete(struct fc_rport  *rport)
{
	struct Scsi_Host *shost = rport_to_shost(rport);
	int timeout = rport->dev_loss_tmo;
	unsigned long flags;

	/*
	 * No need to flush the fc_host work_q's, as all adds are synchronous.
	 *
	 * We do need to reclaim the rport scan work element, so eventually
	 * (in fc_rport_final_delete()) we'll flush the scsi host work_q if
	 * there's still a scan pending.
	 */

	spin_lock_irqsave(shost->host_lock, flags);

	if (rport->port_state != FC_PORTSTATE_ONLINE) {
		spin_unlock_irqrestore(shost->host_lock, flags);
		return;
	}

	/*
	 * In the past, we if this was not an FCP-Target, we would
	 * unconditionally just jump to deleting the rport.
	 * However, rports can be used as node containers by the LLDD,
	 * and its not appropriate to just terminate the rport at the
	 * first sign of a loss in connectivity. The LLDD may want to
	 * send ELS traffic to re-validate the login. If the rport is
	 * immediately deleted, it makes it inappropriate for a node
	 * container.
	 * So... we now unconditionally wait dev_loss_tmo before
	 * destroying an rport.
	 */

	rport->port_state = FC_PORTSTATE_BLOCKED;

	rport->flags |= FC_RPORT_DEVLOSS_PENDING;

	spin_unlock_irqrestore(shost->host_lock, flags);

	if (rport->roles & FC_PORT_ROLE_FCP_INITIATOR &&
	    shost->active_mode & MODE_TARGET)
		fc_tgt_it_nexus_destroy(shost, (unsigned long)rport);

	scsi_target_block(&rport->dev);

	/* see if we need to kill io faster than waiting for device loss */
	if ((rport->fast_io_fail_tmo != -1) &&
	    (rport->fast_io_fail_tmo < timeout))
		fc_queue_devloss_work(shost, &rport->fail_io_work,
					rport->fast_io_fail_tmo * HZ);

	/* cap the length the devices can be blocked until they are deleted */
	fc_queue_devloss_work(shost, &rport->dev_loss_work, timeout * HZ);
}
EXPORT_SYMBOL(fc_remote_port_delete);

/**
 * fc_remote_port_rolechg - notifies the fc transport that the roles on a remote may have changed.
 * @rport:	The remote port that changed.
 * @roles:      New roles for this port.
 *
 * Description: The LLDD calls this routine to notify the transport that the
 * roles on a remote port may have changed. The largest effect of this is
 * if a port now becomes a FCP Target, it must be allocated a
 * scsi target id.  If the port is no longer a FCP target, any
 * scsi target id value assigned to it will persist in case the
 * role changes back to include FCP Target. No changes in the scsi
 * midlayer will be invoked if the role changes (in the expectation
 * that the role will be resumed. If it doesn't normal error processing
 * will take place).
 *
 * Should not be called from interrupt context.
 *
 * Notes:
 *	This routine assumes no locks are held on entry.
 */
void
fc_remote_port_rolechg(struct fc_rport  *rport, u32 roles)
{
	struct Scsi_Host *shost = rport_to_shost(rport);
	struct fc_host_attrs *fc_host = shost_to_fc_host(shost);
	unsigned long flags;
	int create = 0;
	int ret;

	spin_lock_irqsave(shost->host_lock, flags);
	if (roles & FC_PORT_ROLE_FCP_TARGET) {
		if (rport->scsi_target_id == -1) {
			rport->scsi_target_id = fc_host->next_target_id++;
			create = 1;
		} else if (!(rport->roles & FC_PORT_ROLE_FCP_TARGET))
			create = 1;
	} else if (shost->active_mode & MODE_TARGET) {
		ret = fc_tgt_it_nexus_create(shost, (unsigned long)rport,
					     (char *)&rport->node_name);
		if (ret)
			printk(KERN_ERR "FC Remore Port tgt nexus failed %d\n",
			       ret);
	}

	rport->roles = roles;

	spin_unlock_irqrestore(shost->host_lock, flags);

	if (create) {
		/*
		 * There may have been a delete timer running on the
		 * port. Ensure that it is cancelled as we now know
		 * the port is an FCP Target.
		 * Note: we know the rport is exists and in an online
		 *  state as the LLDD would not have had an rport
		 *  reference to pass us.
		 *
		 * Take no action on the del_timer failure as the state
		 * machine state change will validate the
		 * transaction.
		 */
		if (!cancel_delayed_work(&rport->fail_io_work))
			fc_flush_devloss(shost);
		if (!cancel_delayed_work(&rport->dev_loss_work))
			fc_flush_devloss(shost);

		spin_lock_irqsave(shost->host_lock, flags);
		rport->flags &= ~(FC_RPORT_FAST_FAIL_TIMEDOUT |
				  FC_RPORT_DEVLOSS_PENDING);
		spin_unlock_irqrestore(shost->host_lock, flags);

		/* ensure any stgt delete functions are done */
		fc_flush_work(shost);

		/* initiate a scan of the target */
		spin_lock_irqsave(shost->host_lock, flags);
		rport->flags |= FC_RPORT_SCAN_PENDING;
		scsi_queue_work(shost, &rport->scan_work);
		spin_unlock_irqrestore(shost->host_lock, flags);
		scsi_target_unblock(&rport->dev);
	}
}
EXPORT_SYMBOL(fc_remote_port_rolechg);

/**
 * fc_timeout_deleted_rport - Timeout handler for a deleted remote port.
 * @work:	rport target that failed to reappear in the allotted time.
 *
 * Description: An attempt to delete a remote port blocks, and if it fails
 *              to return in the allotted time this gets called.
 */
static void
fc_timeout_deleted_rport(struct work_struct *work)
{
	struct fc_rport *rport =
		container_of(work, struct fc_rport, dev_loss_work.work);
	struct Scsi_Host *shost = rport_to_shost(rport);
	struct fc_internal *i = to_fc_internal(shost->transportt);
	struct fc_host_attrs *fc_host = shost_to_fc_host(shost);
	unsigned long flags;

	spin_lock_irqsave(shost->host_lock, flags);

	rport->flags &= ~FC_RPORT_DEVLOSS_PENDING;

	/*
	 * If the port is ONLINE, then it came back. If it was a SCSI
	 * target, validate it still is. If not, tear down the
	 * scsi_target on it.
	 */
	if ((rport->port_state == FC_PORTSTATE_ONLINE) &&
	    (rport->scsi_target_id != -1) &&
	    !(rport->roles & FC_PORT_ROLE_FCP_TARGET)) {
		dev_printk(KERN_ERR, &rport->dev,
			"blocked FC remote port time out: no longer"
			" a FCP target, removing starget\n");
		spin_unlock_irqrestore(shost->host_lock, flags);
		scsi_target_unblock(&rport->dev);
		fc_queue_work(shost, &rport->stgt_delete_work);
		return;
	}

	/* NOOP state - we're flushing workq's */
	if (rport->port_state != FC_PORTSTATE_BLOCKED) {
		spin_unlock_irqrestore(shost->host_lock, flags);
		dev_printk(KERN_ERR, &rport->dev,
			"blocked FC remote port time out: leaving"
			" rport%s alone\n",
			(rport->scsi_target_id != -1) ?  " and starget" : "");
		return;
	}

	if ((fc_host->tgtid_bind_type == FC_TGTID_BIND_NONE) ||
	    (rport->scsi_target_id == -1)) {
		list_del(&rport->peers);
		rport->port_state = FC_PORTSTATE_DELETED;
		dev_printk(KERN_ERR, &rport->dev,
			"blocked FC remote port time out: removing"
			" rport%s\n",
			(rport->scsi_target_id != -1) ?  " and starget" : "");
		fc_queue_work(shost, &rport->rport_delete_work);
		spin_unlock_irqrestore(shost->host_lock, flags);
		return;
	}

	dev_printk(KERN_ERR, &rport->dev,
		"blocked FC remote port time out: removing target and "
		"saving binding\n");

	list_move_tail(&rport->peers, &fc_host->rport_bindings);

	/*
	 * Note: We do not remove or clear the hostdata area. This allows
	 *   host-specific target data to persist along with the
	 *   scsi_target_id. It's up to the host to manage it's hostdata area.
	 */

	/*
	 * Reinitialize port attributes that may change if the port comes back.
	 */
	rport->maxframe_size = -1;
	rport->supported_classes = FC_COS_UNSPECIFIED;
	rport->roles = FC_PORT_ROLE_UNKNOWN;
	rport->port_state = FC_PORTSTATE_NOTPRESENT;
	rport->flags &= ~FC_RPORT_FAST_FAIL_TIMEDOUT;
	rport->flags |= FC_RPORT_DEVLOSS_CALLBK_DONE;

	/*
	 * Pre-emptively kill I/O rather than waiting for the work queue
	 * item to teardown the starget. (FCOE libFC folks prefer this
	 * and to have the rport_port_id still set when it's done).
	 */
	spin_unlock_irqrestore(shost->host_lock, flags);
	fc_terminate_rport_io(rport);

	BUG_ON(rport->port_state != FC_PORTSTATE_NOTPRESENT);

	/* remove the identifiers that aren't used in the consisting binding */
	switch (fc_host->tgtid_bind_type) {
	case FC_TGTID_BIND_BY_WWPN:
		rport->node_name = -1;
		rport->port_id = -1;
		break;
	case FC_TGTID_BIND_BY_WWNN:
		rport->port_name = -1;
		rport->port_id = -1;
		break;
	case FC_TGTID_BIND_BY_ID:
		rport->node_name = -1;
		rport->port_name = -1;
		break;
	case FC_TGTID_BIND_NONE:	/* to keep compiler happy */
		break;
	}

	/*
	 * As this only occurs if the remote port (scsi target)
	 * went away and didn't come back - we'll remove
	 * all attached scsi devices.
	 */
	fc_queue_work(shost, &rport->stgt_delete_work);

	/*
	 * Notify the driver that the rport is now dead. The LLDD will
	 * also guarantee that any communication to the rport is terminated
	 *
	 * Note: we set the CALLBK_DONE flag above to correspond
	 */
	if (i->f->dev_loss_tmo_callbk)
		i->f->dev_loss_tmo_callbk(rport);
}


/**
 * fc_timeout_fail_rport_io - Timeout handler for a fast io failing on a disconnected SCSI target.
 * @work:	rport to terminate io on.
 *
 * Notes: Only requests the failure of the io, not that all are flushed
 *    prior to returning.
 */
static void
fc_timeout_fail_rport_io(struct work_struct *work)
{
	struct fc_rport *rport =
		container_of(work, struct fc_rport, fail_io_work.work);

	if (rport->port_state != FC_PORTSTATE_BLOCKED)
		return;

	rport->flags |= FC_RPORT_FAST_FAIL_TIMEDOUT;
	fc_terminate_rport_io(rport);
}

/**
 * fc_scsi_scan_rport - called to perform a scsi scan on a remote port.
 * @work:	remote port to be scanned.
 */
static void
fc_scsi_scan_rport(struct work_struct *work)
{
	struct fc_rport *rport =
		container_of(work, struct fc_rport, scan_work);
	struct Scsi_Host *shost = rport_to_shost(rport);
	struct fc_internal *i = to_fc_internal(shost->transportt);
	unsigned long flags;

	if ((rport->port_state == FC_PORTSTATE_ONLINE) &&
	    (rport->roles & FC_PORT_ROLE_FCP_TARGET) &&
	    !(i->f->disable_target_scan)) {
		scsi_scan_target(&rport->dev, rport->channel,
			rport->scsi_target_id, SCAN_WILD_CARD, 1);
	}

	spin_lock_irqsave(shost->host_lock, flags);
	rport->flags &= ~FC_RPORT_SCAN_PENDING;
	spin_unlock_irqrestore(shost->host_lock, flags);
}


/**
 * fc_vport_setup - allocates and creates a FC virtual port.
 * @shost:	scsi host the virtual port is connected to.
 * @channel:	Channel on shost port connected to.
 * @pdev:	parent device for vport
 * @ids:	The world wide names, FC4 port roles, etc for
 *              the virtual port.
 * @ret_vport:	The pointer to the created vport.
 *
 * Allocates and creates the vport structure, calls the parent host
 * to instantiate the vport, the completes w/ class and sysfs creation.
 *
 * Notes:
 *	This routine assumes no locks are held on entry.
 */
static int
fc_vport_setup(struct Scsi_Host *shost, int channel, struct device *pdev,
	struct fc_vport_identifiers  *ids, struct fc_vport **ret_vport)
{
	struct fc_host_attrs *fc_host = shost_to_fc_host(shost);
	struct fc_internal *fci = to_fc_internal(shost->transportt);
	struct fc_vport *vport;
	struct device *dev;
	unsigned long flags;
	size_t size;
	int error;

	*ret_vport = NULL;

	if ( ! fci->f->vport_create)
		return -ENOENT;

	size = (sizeof(struct fc_vport) + fci->f->dd_fcvport_size);
	vport = kzalloc(size, GFP_KERNEL);
	if (unlikely(!vport)) {
		printk(KERN_ERR "%s: allocation failure\n", __func__);
		return -ENOMEM;
	}

	vport->vport_state = FC_VPORT_UNKNOWN;
	vport->vport_last_state = FC_VPORT_UNKNOWN;
	vport->node_name = ids->node_name;
	vport->port_name = ids->port_name;
	vport->roles = ids->roles;
	vport->vport_type = ids->vport_type;
	if (fci->f->dd_fcvport_size)
		vport->dd_data = &vport[1];
	vport->shost = shost;
	vport->channel = channel;
	vport->flags = FC_VPORT_CREATING;
	INIT_WORK(&vport->vport_delete_work, fc_vport_sched_delete);

	spin_lock_irqsave(shost->host_lock, flags);

	if (fc_host->npiv_vports_inuse >= fc_host->max_npiv_vports) {
		spin_unlock_irqrestore(shost->host_lock, flags);
		kfree(vport);
		return -ENOSPC;
	}
	fc_host->npiv_vports_inuse++;
	vport->number = fc_host->next_vport_number++;
	list_add_tail(&vport->peers, &fc_host->vports);
	get_device(&shost->shost_gendev);	/* for fc_host->vport list */

	spin_unlock_irqrestore(shost->host_lock, flags);

	dev = &vport->dev;
	device_initialize(dev);			/* takes self reference */
	dev->parent = get_device(pdev);		/* takes parent reference */
	dev->release = fc_vport_dev_release;
	dev_set_name(dev, "vport-%d:%d-%d",
		     shost->host_no, channel, vport->number);
	transport_setup_device(dev);

	error = device_add(dev);
	if (error) {
		printk(KERN_ERR "FC Virtual Port device_add failed\n");
		goto delete_vport;
	}
	transport_add_device(dev);
	transport_configure_device(dev);

	error = fci->f->vport_create(vport, ids->disable);
	if (error) {
		printk(KERN_ERR "FC Virtual Port LLDD Create failed\n");
		goto delete_vport_all;
	}

	/*
	 * if the parent isn't the physical adapter's Scsi_Host, ensure
	 * the Scsi_Host at least contains ia symlink to the vport.
	 */
	if (pdev != &shost->shost_gendev) {
		error = sysfs_create_link(&shost->shost_gendev.kobj,
				 &dev->kobj, dev_name(dev));
		if (error)
			printk(KERN_ERR
				"%s: Cannot create vport symlinks for "
				"%s, err=%d\n",
				__func__, dev_name(dev), error);
	}
	spin_lock_irqsave(shost->host_lock, flags);
	vport->flags &= ~FC_VPORT_CREATING;
	spin_unlock_irqrestore(shost->host_lock, flags);

	dev_printk(KERN_NOTICE, pdev,
			"%s created via shost%d channel %d\n", dev_name(dev),
			shost->host_no, channel);

	*ret_vport = vport;

	return 0;

delete_vport_all:
	transport_remove_device(dev);
	device_del(dev);
delete_vport:
	transport_destroy_device(dev);
	spin_lock_irqsave(shost->host_lock, flags);
	list_del(&vport->peers);
	put_device(&shost->shost_gendev);	/* for fc_host->vport list */
	fc_host->npiv_vports_inuse--;
	spin_unlock_irqrestore(shost->host_lock, flags);
	put_device(dev->parent);
	kfree(vport);

	return error;
}

/**
 * fc_vport_create - Admin App or LLDD requests creation of a vport
 * @shost:	scsi host the virtual port is connected to.
 * @channel:	channel on shost port connected to.
 * @ids:	The world wide names, FC4 port roles, etc for
 *              the virtual port.
 *
 * Notes:
 *	This routine assumes no locks are held on entry.
 */
struct fc_vport *
fc_vport_create(struct Scsi_Host *shost, int channel,
	struct fc_vport_identifiers *ids)
{
	int stat;
	struct fc_vport *vport;

	stat = fc_vport_setup(shost, channel, &shost->shost_gendev,
		 ids, &vport);
	return stat ? NULL : vport;
}
EXPORT_SYMBOL(fc_vport_create);

/**
 * fc_vport_terminate - Admin App or LLDD requests termination of a vport
 * @vport:	fc_vport to be terminated
 *
 * Calls the LLDD vport_delete() function, then deallocates and removes
 * the vport from the shost and object tree.
 *
 * Notes:
 *	This routine assumes no locks are held on entry.
 */
int
fc_vport_terminate(struct fc_vport *vport)
{
	struct Scsi_Host *shost = vport_to_shost(vport);
	struct fc_host_attrs *fc_host = shost_to_fc_host(shost);
	struct fc_internal *i = to_fc_internal(shost->transportt);
	struct device *dev = &vport->dev;
	unsigned long flags;
	int stat;

	spin_lock_irqsave(shost->host_lock, flags);
	if (vport->flags & FC_VPORT_CREATING) {
		spin_unlock_irqrestore(shost->host_lock, flags);
		return -EBUSY;
	}
	if (vport->flags & (FC_VPORT_DEL)) {
		spin_unlock_irqrestore(shost->host_lock, flags);
		return -EALREADY;
	}
	vport->flags |= FC_VPORT_DELETING;
	spin_unlock_irqrestore(shost->host_lock, flags);

	if (i->f->vport_delete)
		stat = i->f->vport_delete(vport);
	else
		stat = -ENOENT;

	spin_lock_irqsave(shost->host_lock, flags);
	vport->flags &= ~FC_VPORT_DELETING;
	if (!stat) {
		vport->flags |= FC_VPORT_DELETED;
		list_del(&vport->peers);
		fc_host->npiv_vports_inuse--;
		put_device(&shost->shost_gendev);  /* for fc_host->vport list */
	}
	spin_unlock_irqrestore(shost->host_lock, flags);

	if (stat)
		return stat;

	if (dev->parent != &shost->shost_gendev)
		sysfs_remove_link(&shost->shost_gendev.kobj, dev_name(dev));
	transport_remove_device(dev);
	device_del(dev);
	transport_destroy_device(dev);

	/*
	 * Removing our self-reference should mean our
	 * release function gets called, which will drop the remaining
	 * parent reference and free the data structure.
	 */
	put_device(dev);			/* for self-referenceFibe
	return 0; /* SUCCESSport}
EXPORT_SYMBOL(fc_vport_terminate);

/**
 * icon Grapsched_delete - workq-based righ Threquestcifica reserrigh@ pro:	reser to be free sd.
portstatic void
ts reserved.
  righ T(structsbute_ublic L*bute)
{
	e as puts reser *fy
 So=
		container_of(bute, free the Ftware,an redeneral _lishe;
	int der ;
by
 at =hs of the hics, Inc(reserour of (lInc.
		dev_printk(KERN_ERRcense, ->dev.parent,
			"%s: %s could not modifyneralcrea witvia "eralbshost%d channel %d - error %d\n", __func__neraled iname(&t will mod) iICULAR ied w->the
_noion 2 CULAR anty ofrsioagram}
 AllistrBSG supse, ist/e det istrts destroy_bsgjob - routine/or teardown/free soa fc bsg jobistrijob:	fc cop_icenthat isGenemodtorn Pe ast unon)  of hicss oeived arogry o Freec Lbuter mo; i*jobed byunsigned long flagsany lpin_lock_irqsave(&job->y of rig,111-13ram is d Generef_cnt) {
	 USA
 uneneral==restor=== GeneralCopyave r (C) 20d/or sy;
	} , Emulex Corporationneral  Rewritecificed w, tarerCnty of
tra Genens Gra/* releassoftutes expyou thvporware; ortedkfreuncattrinclude_payload.sg(at r mo x/module.h>
#plyde <linux/initscsi/inclui_des more d have recace, SudonThiscompleattr of GNU Gyou   alincludesifNY W*/
#LLD hasistrcsi/sccsi/csi/_cm_transpthe
ith thiLicence, Suianspo, wdnclude e Software
 *  Foundde <scsi/sc9 Terans Place, Su   s330, B
 *  the <scsi/sreq =  02clude;i/scsi_cbsg_fcscsi/sp =#inc->next_ri_proptierrandMERC<scsi/"csi/->ERCHAsic int fc_plye "sculic y004-2er t< 0ibu *  we're only t, devingFibe#opti*, fielOR Art_scheplyportr optifc_wareit w_len = sizeof(uint32_h>
#ielset * pro);
der thework);
v<scsi/scsi_len"

s *);
 assume all#include _device was funnsferred,d.
 idual == 0ruct ;
staon Grhannel,0 ublic  rspSmart,WARN_ON <scsi/scsisi/scsi_cnel tr_rcvork);>strunt channelb "

spdevse
#incstr(bidi)con Gra **dd(s*_rportsg_portt -= min*,ostadd(sfc_ed w_attrsdev, der thneral	csi/sc*);
static voiadde, ablk_de <net/_include(reqsiuct rnclude <scsi/vporirq_i/scsi_ can ha i/scsclude <scsi/ation, n Grapscscsi/scsi_ct withrq:de <scsiU Geinclude anspoholds,
	e,
	 write tifion,  <scsoftware
 *  F f notwenamed ave sa9 Templeinclude "sq30, Bion 2 of th>
#include i_trder pecial; Boston, MA  02ght (C07 and re Corpora table  Rewrite for host, targent fcifie_ht (C |= FC_RQST_STATE_DONEiprognst James --in td remote  voidquesructes,ibutes,der tst
 *  "

slic endtdev, 
/_all(rq,,_shoue_worueueund attr  Thi., 59scsi_cdefine so that we<sc_timeout - handlerscsi/when a/ed w object ].vasouou srireq:der _ATTR(_prele[id .A PA;oftware
 enum ULL;	hile[ir_t, devde <scsi/scrn naluettr_##				fix##_##e_A PA = 	\
	_reak;		A PA,_mode,(el tr*ublishow,_on
 e)
e as puSsi_cHotabl the
el,_motr the
#_match(cofc_internc_rpi = to_pe * fc_e)	(re
 
 *s,_moeserh>
#i
#define fc_enum_namealueal.hsg_h,
	int g+) {	add(sint fcware &&y,		\
uCULAR  the Gchar*vpoFC_(c) e)			(BLOCKEDructt, devsBLK_EH_RESET_TIMERme_search(title, table_type, table)			\
static 04-20me_srantr *get_f&##title##	table				nt i;e); i+1t nst chast *ied e tab++_key)	\
{	ype valu\r optiirt svalues t						, tableN04-2!ailur&& i->f->etline fc_ect Scs/* ccon LLDDGeneaband/ohe i/o as itludeues t	}			c void_SIZE(stru Graphype	valilure +	add(swoint i	nre
 *hopee_attr"ERROR: FCC_DEVICEreak;	e fc_e ==identiimp	"		"Unkfailnclude  statusNTABILITE_OTeue(s*trucget/h	ret			sync_io() doesn't checko-poiue_woNuct 04-2 {
	chlen)*vpor) {ues NOT_HANDLED/						\
}},
	{ FC_PORTTL(c) ,	"L}
ftware
 intde <scsimap_buffernetlink_fc.h>
#opti(d *bunse fn 2 o table_				\s, ablize_t szr thtargetetr_##_prcatter#incltes,ns...
_phys_segmul,
e = NBUG}


!own",N,		"Unknown")
#definbuf->h>
#inc = kzalloc(sz, GFP_r" }EL) 2004-2!MAX_NAMELEN	p)" },
	{ F-ENOMEMm tag_t/ne_	"NPe(nction for vicon s)
#define FC_PORTTYP_nction fcn		50nt)"rqTo-Posg rig->\
	feq, ule.h>cificvpConvert uest_queannel,vent_cobytes righReet, devsfs_hostd have recreq_toInc., 5f tAevenate/hlue e"NLPo, table)			e as pur
sta
Fibeif (table[i/scsi_cmnn			name = tame;		 the
:	SCSI 					correspondn Graor-poin
#de = t							eser:	(opse_qul)TYPER
{				Pare 		"link_Publ" },
	{ FCH_EVT_LIP			*v,_typeNOTPRESENT,					needs a\
staADAPTE,string na_POR	P,	"Po A PA;
}struct_				_nae_ty						port__,
	{ FC_Pf tPublnc*,,			,priv.h"
#include "si, MAe = 	\
	__AT				\
	ialues type values to ascii string naT_LINKiv.h"ude <scsi/ fc_qd.h>
#iGrappe *vaetlink_fc.h>
#include < ARRAYre, strknown"_ FCH						\
e = N
stati All Reus};
fcT_PORT_O, table)		) +WN_PORUddttribuiz;			  		"Unknown" 				\fude <sine FC_Ht_scdefinent-t  Fi Note:<linunclua bit silly* ConvT_sche<scsi/getsscsim_seade taa SGIO v4 ioctn Grapiue(swhichnamectluesm_NAMrest, int stadd(sblkrgettat" },
ue;ar			
stat
	cha{ FCH_target = {TNESS al		break. And onnown" },
	{,v, sevicname *o wrapoidstrud.
 *
 s suC_PORatmart				\thinkes[]  *ida _EVT
	cha	"NPor.  Iher" s[]isE_OFFLllLiceth itto asb} fc_por#title##ey,		 table_tyt_onkey	
	{ FC_		if FCH_EV= _PORT		"NPenownringnknopc viat ree
	enum dle, CODEle.h>sd_data int change_typ[1]fc_port_eneralnite_type, table)	 theble_tPORTSTA;
	{ ,
	{ FC_PORT,sg_fc.h>
)es tocmdnlinT_name(LOOPtadd(s{ETED,		"el trost *shos_Hos},
 fc_int chametic rch(pted" },_LIN_SENSE_BUFFERNKNOmes)
Size,
	{ nt c	\
	feort_	{		chat_codt evebrthe iaRTSTAbioct Scsreer versnt-ode vnectioi_type,ude <scsi/sviceasciiOnlinTTYPren Gra	gotoate	vEN	3rls, Sua poit		\
tru				(stru{ FCTSTA	chaiit_stvpor, tab*/ fc_port_stast_attrs *),]	\
{[] = {
	{ FC_name *RTSTATE_	{ FCHrq{
	enum fcort_st  Rewrite {
	ee;
	<scsi/scsi_cor *na"_PORT		"DINKDOWev = &ypas, tdev/						\
}


/*LIZING,t i;			,	"By_genalizingerum_naor FNKDOW...
 t-to-akit worts, et		"lip" }uct {
	enum fc
ent_c
statirknow	FC_PORT	 },


_DISABLED_PORDisNOSTd" }:ce.hlude <scsi/ct {
	enum fc.h>
#includ_DISABLED,		_vp_V(c) 2FABRI{ FC_PORT		MELEN	30
OWN,alues< ARispable_.
 *
 *{
	FC"FabPATCH_BREAK,				rt.h, dev, qnclulip"(strRTSTA from q loopon GratPORTTYn Gri].m_seaMELEN	24
nown" },_pors)ELENARRA fc_ticEnvert fc_or v24
UNnt_code,e fc_vport_state enmote prt" },
name */ort_l}UT,	ine so that we_SUPPport_sta -buteces				{	"Byphost objectsh"

sgtid_bindtoISABL_NAMEq:			{ FC_VPTE_LOOPBqueu	{ FCH_EVT_ADNE,sed" },fware attaNU G_VPOude <linux a  02write tge FC_HO)ues stadd(sdhost_evenort_state,rt_sde <scsi{ FCte */
stanum_name_match(t
stat *q,
	{ FC_PNLIue;
	"t_staonl FC_ine" },
	{ 	\
		if (table[priv.h"
#VT_ADART_FAILED,CPort Namefavpor,
	{ FCH_EVT_ADAPTNK_ernacmdannel,
};
fc_tupPublic  },
};artN
	{ Flengthnamemsgcodxt_staunique,
	{ /* Valistruc[] =	"Bypcomm	"Unown"swistatABRIC_REJ_WWN->*/
statct Sccd vpFC_BSG_HST_ADD_R
			:ound_id (F+C AddressFC_Poopback" } fc_tgddH_EVT_Onlin	30
 AILE*/
stat FC_PORTTDEL */
statinnding_
 *  4 },
	{ FC_c_bitfielC_TGTdelLEN	30
##tiBIND
	{ t_last_state 30

ELS_NOLOGINfc_bitfield_name_search(title, table)			ce fargePORTp_hosbe_bindmoda xmt		"Un
statifierssc void04-2VENT_C_REJ_WWN,	"Fabricble_tyublic) ||
the teCH_EVT_LIst_attrs *).=sfs.i or_unct ScsORT_UNK-EINVAL Wid_VPORT_DISle)			msg Wid}ize_t							\
get_fc_##titleC fc_bitfield_name_search(title, table)			c ssizefix = ""ii str\
	ss VPOt fc_v0		\
	len o ascii strinEVT_LINK_retuific(		\
able[i].vaAYUNKNOWGNOST{ FC_PC_PORTT	{ FC_GNOST[ine fc_e &AGNOSTIc_poo ascii 	t fc+= sn thef(buf +rint, "%s% FC_EVT_LIe_matc,		"NVENDORfc_bitfield_name_search(title, table)			vendon thpii strt i;			  GNt->"Class_idme,	rLt FC_COS bitm fu_host_evenut" }	"Li.h		"Clas.OS_CLAS 3"!FountateCOS_CLASS3,6,	"Class
static const stSRCH{
	u32 			value;
	char			*name;
} fc_cosdefaultitleconst stBADR2" }2 			value;
	char			*e */-to	{ FC_ifPresreallysent"_[i]. Fabric Reso{ FCnFCH_fc_vport_stET Logout *  Td" <_stat (FC_VPORT_UNKMELENSGst, int = 0; i < AR" },
u32 	RT_UNKN,		"Unkno		\
			* LogouVT_EVE,
	{ FC_code		st_last_sAMELEN	30
 ;,	"F,
	{
	char	:/
sta_code		NLPvoidnRTSTATKNOW_NAMEmarthr the tline" town"};
fc_ert_state)
#defi< Address)", 7 },
t_stat_name(struct request_queue *);
 i++)  fc_hobitete(s_ *
 *rt_sIAGNOSTICS,s)
#defineAddress)", 7 },
} S,		"Linkdown" n	{ FC_PFC_POFa10 Gbit,
	{ FCH_E Wideue;
	cailsTGTID_BgooseWPN, "w-d.
 _PORTRTSTAT++)
		in */
stame(OFFstop,	"DideATE_DIipRTSTATrite t			valuype_names[] = el tde <scsi4(at t++)
	rch(title, _EVT_ADAstatne" }ernaht (Cer th
#define fc_enum_name_s};
fPECULAR ut" }qTE_Lnvert AILEportSueset",	"Initialie = Nearch(title, tablePORTSPOGOUTue->
state for host, targKNOWN,	RIC,Y_WWbit(QUEUE_FLAG_REENTER,Gt_role_nRGHANG"FCP Tarost, ta&&NKNOW!CP_INITIATORRT_ROLEole_nator0; i < FC_FC4_L_ROLE_IPalues,},
};
fKNOWN,	TE_Lfc_port_ro_se;
fc_bitfield_name_sea_FCP_TARGET,	;;ype,
	enuuntatic F_id.ndin FC_are rum fu
/ righD FC_POrolFC_Vclbindefins,_stde <to					MASK.
 */
#defineport_type values to asc_FCP_TAC_PORT_ROLE_IPge 0; i < FC_Fd service funt_D_MA enum []	{ FC_VPORT_	*"4 Gb wilorarch(title, tabl vRTSTATORT_UNKNOWN,		"Unknown" },
	{Gbit" },
	{ FRTSTATme_search(title, tabl },
	{ FC_VPORT__DISABLE "\nletetabl
32 			value;peed	\
	chs#incruct work)" }	{ FCH_search(title, table)			e,
		mart FC_Fi++, fcIND_NONE, "none"D		0x0ructc
#dc void fc_sBP_IN++)
		wpn (World Wide FC_POName)", 4; i < FC_FTGTE bit values_e...
 * Incruct *work);
N/*
 * nttribute countNar * object type...
 * Incruct *work)ID, t Nametype,C Addressect 7
	{  fc_host_evenort_state,search(tit FC_PORTTYsearch(title, tabl
 *  ted_rio(struct work_struct 4,	"Class 4"meORTTYPE_M_ATTRS	22

stRP	\
	rec_bitfield_name_search(title, tablrt(str					preVPORT	{ FC__del				\
get_fc_##t  Finame *funcort EVT_Le_matc				, 			\
	rt(str,
	{ Factt **aii st"port_uen;			2 			value;
	char			*name;
\n"ort s\ed to sys_fai  struct}" },
	{ FC_VtFC_FCOSte vaGET,	C_VPORT_UNKNOWN,		"Unknown" },
	{Gbit" },
	{ FC_PORTSPARGET,	rt(strar			*name;
} fc_cd host are are
	 * part of the midlayeis#define FCd. Values arFC_VPORT_DISABLED,		"Unknow);
static *work);
statiELETEPEED_1GBITPort1(i = 0; i < FC_FC4_L_STARG2T_NUM_AT2(i = 0;t i;midlion; e.
TARG4T_NUM_ATRTID;
	struct device_attrib10T_NUM_ATT (i = 0; i < FC_FC4_L_STARGbute cont"8	struct device_attribute pr6T_NUM_ATT6	struct device_attribute pYPE_NEGO};
f Log"Not Negoame_	{ FCATTRS	9
_speed_na_port_state, private_s)
#define ate_starget)

st, int cha
c_## FCHfc4s (						buf, u8 *fc/* Coned byo asc,* Th=0;
					\
}


/* ConvFC_FC4_LIe##_IIRSRVR			FCIDwork);
c const - generic=c const stru		name = taC_DEV/t;
	st fc_internal tfmanagtic void fc_tNLINE,		"prelt fc_	{ FCH_EVT_ADAPTER_CHANGlen +FC rEVT_ADst *sho(c) 2NUMreak;S + 1];
}; _f th 0; i_bitfdev: FORice_ADAPTEKNOWen +EVT_ADAPTER_C	"rs		\
		if (table[);
	return le_at_struct *work);

/*
 * Attribute counts pre object ty(table[{ FC_* Thireas },
eserget andifue *);
*rgetNN, "wwnn (	"port_offliT_ADAFC_COS_UNIQUE,	""Clasr_{
	{ FC_TGTID_BIND_NONEt_tgtid_T_EVEe;
} fc_porrgettruct device_awhilpportlk

/*
 _plugged(q
static
statiVPO				RvaluesIg name *)			\
	return ng name *tablstruce_t							TRS]" },
	{festate,.  Alles[] = {
	\
	chtructo known , fchlenY WA i;		     {									i!terface. As sucOWide 
static co_bini = 0k(s-ENXIOEVT_Lport_type value(x00000e
#defi				FN
	ret FCH_EVT_LNEGOT_rport(,},
	{ FCtruct reevrt->poearch(title, s *pret) = c voi = -t_fc_vpo		*nam & tEmulmote port et_port_name(starVPORT_UNKNOW
	{ FCH_EVT_ADAme)",i_targetport_stateRGET,	ort_name(stargetrt_nder thS	10
#dice, csi_mart	fcretet_nORTSTAmt_st_port_nam-1;_name_      N"Unkno
			       NULL,
			       NU
statinum_name_sea#_FC_VPOR
	struct de	{ FC_ATTRTTRS	22/
statate FC ate	vE_	FC_STARGET_NUM_ATTRS];ort_attr_cont;
	static t transport_container rport_attr_cont;
	struost *shosce_attribute private_rport_attrs[st(ied w\
			/*
TTRS];
tribute *stct device_attribute *rport_attrs[FC_R" },RT_NUM_ATTRS + 1];

,
			       NULL);

static int fc_host_setup(struct fix = iuct workeh>
#incsCULARate enuv
 * 000e
#defid byublic ut noRT_IORT_UNKNOWN,		r FCH_TTYPE_Ok_Attr_TRANSfc_hoCL_namezin				\
}ORT_UNKNOWN,		t type...
 * Iame_CH_E-> LogouetistidiUnknown" }_fc4rgetort iine" ca_NLPne FC_TI strmor22

sttransporinterfae fc_vport_staULL);etup,
			 4s, 0
 * port_se {
	d  *cdevvd
	fc_

static LL,
		 FCH_##tiM_ATTRS];	{ FC_POst->s			       NULL);

static int fcown" NULL);

staite_n_port_name(stargservice fun },
	ttrs[
							FC>port_id = -1;
	f    "ques_STAR;
	s fc_tuct devtriv" }, Addrequest[but noR you caet and " },
	{ FC_Pode_namesearch(title, transport_container ELETED,		"UN_struct *work);

/*
 * Ac void*c voiNLINE,		"port_onpass_ROLE_I{ FCed_fion; e *tc_remove(surn _host->suport_,_NO Node N fc_t rpor ic ssizeDttr_cont;
	stt(strEED_UNKKNOWN;
	f(buet -1;t->supactiveribut, st->szeof(fc_host->syWPN;a];
	structport_typenum ARGET_N *t->system_hotname))host->tgt},
	{ e, table
			 m = to_scsi_targymbolrt_bindxfra table_tyemset(fc_host->symbs);
	Ito_umber, 0,fc_,vport_sehost->tgtsfc_host->vpo) = 0m_targetWWPN;
d
	{ FC_PO 0, sizeof(fc_hothe
ad*  MC enum 	"Unted 	{ FCH_Ehooks soPresca	memceiv
#includeRT_NUMid fc_tit->syne Ffcts =0_bitf>suphosnux/nt->syan" },
	hnt channelt wrihost_e_type, e_nam"nget_

/*

d Wide ame(stPort Name)",porte	{ FCH enum uest_qd to syame = -1;
	mme = -1namecontc_host->system_hostFab{ FCH_EVT_ADAWide Node Name)", 4 },
	{ FC_TGTID_BIND_BY_ID, hostnamfine tn

/*
 * A*valueY_SI"name e
fc_ A PA[20]_target i;			RGET,	 =;

	frget(dev) + 1];

	struct d;
	char			_statTfc_tg7  Un		"Otf(hread_wo,=olic_toorkqueue)knowa"VT_LINf%d"host->tS_CLASU G_vpor be __f (!f			ocners forst->wohost->tgtwoINIT_LIS_HOST_EVEqv = 0s		"Otheearch(port, fc_hosr:ress)				\fork_ {
	stNto_q)
		reR,		ial;
fc- noWWPN;

	INIT_LfabrT_LIST_= ort_i
	;

	snprE_MAX_NAMELEN	30
    Nhost->vportspasst;
	strORTID_MASK	hatde_nameed
fc_bitfieldBIDI,defineattrs[uses[FC_truct devqied w)
#dute device_aemove(struct rrqown" d_	20
stc_vpCH_E,
			 s_hos detected ;
staticemoid = 0;et(fEFAULTnown"t->dOUTurn -UNKNOWports)gisalueers foq,>sup,torkqueue);

	fe enum fc vi  als)
#ERCHAN-ribujuLINEa_NLPdo sgio },
ed to sysfsute t, int channelct reremote po"t_HEAD(&fc_hosteof(fc_host->symbolame =leanup
 *     ymbolic__nam" },si   Nhost->wost->tgtNOMEq =RIC_LOGOUTimeout_deleted_);

statted snue;
	chost, fc_hosrk_q->vportarget_id = 0;
	f			       N)
 *  k_q wq_%d",portescsi_ttructort_name = -1;
_name =st *s" },
	{ F       NULa				\ },
 table2007truckport-(fc_rport_clasrt_tyNASS(name);
	if (!fc_host->work_q)
		re>devloss_wor		       NULL,
devloss>port_role_namezin  NULL);

/*
 * Mo Remove actions fdl virt->rqst	fc_h;
	f = 0     NULL);

/*
 * Mo	   thoutRT_C
host-> the FC transport
 *  emove rt(sf (!t the FC transport
 *  Smart	
#defiice,
static DE+) {			\
 = shostr = 0;NIDAPTSign_hos{
		ost_remove,
			       ut W * Setup and Remove actions for remote portsmovt_stame =HEAD(&fc_host-dev->kobj.d by;LARE_TRANSPde
	fc_	t->symbolic__namcnspome(eatic *fc				\
}


/* Cost-_host-for stansporame = -1struct request_qt_class,t->rqst FC rt_clasy detected atic DECLARve    NULL,
rqstIWUS and Remove actions foDECLARn noANSructportASS(ote porcClastatic DE_BLOk_q = NU"_DEVICE_BLOC_hos
	fc_EOUT.");

/*
 *_loss_tmo, _DEVICE_BLOCuf +/* secondsetup lude <_parac_funcdansp_
/*
 tff0
#ARE_TRANSPORT_RT,	, S_IRUGO|S_IWUSR);
elowT_NUMved. Value should be"supporttwee remote port DEVICE_BLOCK_MAode_na"UnknsMEOUT.");	 " ortsDeral UM_ATT       NUL_fc_cthe
s/r = 0container Fabric Res

/*
 *>
#inclu    sto2 of theOSE.oftware
 *  Foundibutesbe eT_HEAD(&fc_host->rport_bindin04-2 int fcorkqun_MAX_TIMEOUT.");rch(titlert s:ibutesWC_STuld}        Origiype_Author:  Martin Hicket(fcMODULE_AUTHOR("pe tabct Sc");n fc_n oDESCRIPTION("FC TFCH_EVT_ A
	int i;	rans:		ransLIprint("GPLrans*
 * fcc_bi	k_q  FCH_EVT_ get_);edFC_VPOrt_f_hos
	enunumbePORT);
