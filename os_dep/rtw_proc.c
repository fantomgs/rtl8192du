/******************************************************************************
 *
 * Copyright(c) 2007 - 2013 Realtek Corporation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110, USA
 *
 *
 ******************************************************************************/

#include <drv_types.h>
#include "rtw_proc.h"

#ifdef CONFIG_PROC_DEBUG

static struct proc_dir_entry *rtw_proc = NULL;

inline struct proc_dir_entry *get_rtw_drv_proc(void)
{
	return rtw_proc;
}

#define RTW_PROC_NAME DRV_NAME

#if (LINUX_VERSION_CODE < KERNEL_VERSION(3,9,0))
#define file_inode(file) ((file)->f_dentry->d_inode)
#endif

#if (LINUX_VERSION_CODE < KERNEL_VERSION(3,10,0))
#define PDE_DATA(inode) PDE((inode))->data
#define proc_get_parent_data(inode) PDE((inode))->parent->data
#endif

#if(LINUX_VERSION_CODE < KERNEL_VERSION(2,6,24))
#define get_proc_net proc_net
#else
#define get_proc_net init_net.proc_net
#endif

inline struct proc_dir_entry *rtw_proc_create_dir(const char *name, struct proc_dir_entry *parent, void *data)
{
	struct proc_dir_entry *entry;

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,10,0))
	entry = proc_mkdir_data(name, S_IRUGO|S_IXUGO, parent, data);
#else
	//entry = proc_mkdir_mode(name, S_IRUGO|S_IXUGO, parent);
	entry = proc_mkdir(name, parent);
	if (entry)
		entry->data = data;
#endif

	return entry;
}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5,6,0))
inline struct proc_dir_entry *rtw_proc_create_entry(const char *name, struct proc_dir_entry *parent,
	const struct proc_ops *pops, void * data)
#else
inline struct proc_dir_entry *rtw_proc_create_entry(const char *name, struct proc_dir_entry *parent,
	const struct file_operations *fops, void * data)
#endif
{
	struct proc_dir_entry *entry;

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5,6,0))
	entry = proc_create_data(name,  S_IFREG|S_IRUGO, parent, pops, data);
#elif (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,26))
	entry = proc_create_data(name,  S_IFREG|S_IRUGO, parent, fops, data);
#else
	entry = create_proc_entry(name, S_IFREG|S_IRUGO, parent);
	if (entry) {
		entry->data = data;
		entry->proc_fops = fops;
	}
#endif

	return entry;
}

static int proc_get_dummy(struct seq_file *m, void *v)
{
	return 0;
}

static int proc_get_drv_version(struct seq_file *m, void *v)
{
	dump_drv_version(m);
	return 0;
}

static int proc_get_log_level(struct seq_file *m, void *v)
{
	dump_log_level(m);
	return 0;
}

static ssize_t proc_set_log_level(struct file *file, const char __user *buffer, size_t count, loff_t *pos, void *data)
{
	char tmp[32];
	int log_level;

	if (count < 1)
		return -EINVAL;

	if (buffer && !copy_from_user(tmp, buffer, sizeof(tmp))) {

		int num = sscanf(tmp, "%d ", &log_level);

		if( log_level >= _drv_always_ && log_level <= _drv_debug_ )
		{
			GlobalDebugLevel= log_level;
			pr_info(DRIVER_PREFIX "set GlobalDebugLevel = %d\n", GlobalDebugLevel);
		}
	} else {
		return -EFAULT;
	}

	return count;
}

#ifdef DBG_MEM_ALLOC
static int proc_get_mstat(struct seq_file *m, void *v)
{
	rtw_mstat_dump(m);
	return 0;
}
#endif /* DBG_MEM_ALLOC */


/*
* rtw_drv_proc:
* init/deinit when register/unregister driver
*/
const struct rtw_proc_hdl drv_proc_hdls [] = {
	{"ver_info", proc_get_drv_version, NULL},
	{"log_level", proc_get_log_level, proc_set_log_level},
#ifdef DBG_MEM_ALLOC
	{"mstat", proc_get_mstat, NULL},
#endif /* DBG_MEM_ALLOC */
};

const int drv_proc_hdls_num = sizeof(drv_proc_hdls) / sizeof(struct rtw_proc_hdl);

static int rtw_drv_proc_open(struct inode *inode, struct file *file)
{
	//struct net_device *dev = proc_get_parent_data(inode);
#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 17, 0)
	ssize_t index = (ssize_t)PDE_DATA(inode);
#else
	ssize_t index = (ssize_t)pde_data(inode);
#endif
	const struct rtw_proc_hdl *hdl = drv_proc_hdls+index;
       return single_open(file, hdl->show, NULL);
}

static ssize_t rtw_drv_proc_write(struct file *file, const char __user *buffer, size_t count, loff_t *pos)
{
#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 17, 0)
	ssize_t index = (ssize_t)PDE_DATA(file_inode(file));
#else
	ssize_t index = (ssize_t)pde_data(file_inode(file));
#endif
	const struct rtw_proc_hdl *hdl = drv_proc_hdls+index;
	ssize_t (*write)(struct file *, const char __user *, size_t, loff_t *, void *) = hdl->write;

	if (write)
		return write(file, buffer, count, pos, NULL);

	return -EROFS;
}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5,6,0))
static const struct proc_ops rtw_drv_proc_ops = {
	.proc_open = rtw_drv_proc_open,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_release = single_release,
	.proc_write = rtw_drv_proc_write,
};
#else
static const struct file_operations rtw_drv_proc_fops = {
	.owner = THIS_MODULE,
	.open = rtw_drv_proc_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
	.write = rtw_drv_proc_write,
};
#endif

int rtw_drv_proc_init(void)
{
	int ret = _FAIL;
	ssize_t i;
	struct proc_dir_entry *entry = NULL;

	if (rtw_proc != NULL) {
		rtw_warn_on(1);
		goto exit;
	}

	rtw_proc = rtw_proc_create_dir(RTW_PROC_NAME, get_proc_net, NULL);

	if (rtw_proc == NULL) {
		rtw_warn_on(1);
		goto exit;
	}

	for (i=0;i<drv_proc_hdls_num;i++) {
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5,6,0))
		entry = rtw_proc_create_entry(drv_proc_hdls[i].name, rtw_proc, &rtw_drv_proc_ops, (void *)i);
#else
		entry = rtw_proc_create_entry(drv_proc_hdls[i].name, rtw_proc, &rtw_drv_proc_fops, (void *)i);
#endif
		if (!entry) {
			rtw_warn_on(1);
			goto exit;
		}
	}

	ret = _SUCCESS;

exit:
	return ret;
}

void rtw_drv_proc_deinit(void)
{
	int i;

	if (rtw_proc == NULL)
		return;

	for (i=0;i<drv_proc_hdls_num;i++)
		remove_proc_entry(drv_proc_hdls[i].name, rtw_proc);

	remove_proc_entry(RTW_PROC_NAME, get_proc_net);
	rtw_proc = NULL;
}

static int proc_get_mac_reg_dump(struct seq_file *m, void *v)
{
	struct net_device *dev = m->private;
	_adapter *adapter = (_adapter *)rtw_netdev_priv(dev);

	mac_reg_dump(m, adapter);

	return 0;
}

static int proc_get_bb_reg_dump(struct seq_file *m, void *v)
{
	struct net_device *dev = m->private;
	_adapter *adapter = (_adapter *)rtw_netdev_priv(dev);

	bb_reg_dump(m, adapter);

	return 0;
}

static int proc_get_rf_reg_dump(struct seq_file *m, void *v)
{
	struct net_device *dev = m->private;
	_adapter *adapter = (_adapter *)rtw_netdev_priv(dev);

	rf_reg_dump(m, adapter);

	return 0;
}


ssize_t proc_reset_rx_info(struct file *file, const char __user *buffer, size_t count, loff_t *pos, void *data)
{
	struct net_device *dev = data;
	_adapter *padapter = (_adapter *)rtw_netdev_priv(dev);
	struct dvobj_priv *psdpriv = padapter->dvobj;
	struct debug_priv *pdbgpriv = &psdpriv->drv_dbg;
	char cmd[32];
	if (buffer && !copy_from_user(cmd, buffer, sizeof(cmd))) {
		if('0' == cmd[0]){
			pdbgpriv->dbg_rx_ampdu_drop_count = 0;
			pdbgpriv->dbg_rx_ampdu_forced_indicate_count = 0;
			pdbgpriv->dbg_rx_ampdu_loss_count = 0;
			pdbgpriv->dbg_rx_dup_mgt_frame_drop_count = 0;
			pdbgpriv->dbg_rx_ampdu_window_shift_cnt = 0;
		}
	}

	return count;
}


int proc_get_rx_info(struct seq_file *m, void *v)
{
	struct net_device *dev = m->private;
	_adapter *padapter = (_adapter *)rtw_netdev_priv(dev);
	struct dvobj_priv *psdpriv = padapter->dvobj;
	struct debug_priv *pdbgpriv = &psdpriv->drv_dbg;

	//Counts of packets whose seq_num is less than preorder_ctrl->indicate_seq, Ex delay, retransmission, redundant packets and so on
	DBG_871X_SEL_NL(m,"Counts of Packets Whose Seq_Num Less Than Reorder Control Seq_Num: %llu\n",(unsigned long long)pdbgpriv->dbg_rx_ampdu_drop_count);
	//How many times the Rx Reorder Timer is triggered.
	DBG_871X_SEL_NL(m,"Rx Reorder Time-out Trigger Counts: %llu\n",(unsigned long long)pdbgpriv->dbg_rx_ampdu_forced_indicate_count);
	//Total counts of packets loss
	DBG_871X_SEL_NL(m,"Rx Packet Loss Counts: %llu\n",(unsigned long long)pdbgpriv->dbg_rx_ampdu_loss_count);
	DBG_871X_SEL_NL(m,"Duplicate Management Frame Drop Count: %llu\n",(unsigned long long)pdbgpriv->dbg_rx_dup_mgt_frame_drop_count);
	DBG_871X_SEL_NL(m,"AMPDU BA window shift Count: %llu\n",(unsigned long long)pdbgpriv->dbg_rx_ampdu_window_shift_cnt);
	return 0;
}

int proc_get_wifi_spec(struct seq_file *m, void *v)
{
	struct net_device *dev = m->private;
	_adapter *padapter = (_adapter *)rtw_netdev_priv(dev);
	struct registry_priv	*pregpriv = &padapter->registrypriv;

	DBG_871X_SEL_NL(m,"wifi_spec=%d\n",pregpriv->wifi_spec);
	return 0;
}

static int proc_get_mac_qinfo(struct seq_file *m, void *v)
{
	struct net_device *dev = m->private;
	_adapter *adapter = (_adapter *)rtw_netdev_priv(dev);

	rtw_hal_get_hwreg(adapter, HW_VAR_DUMP_MAC_QUEUE_INFO, (u8 *)m);

	return 0;
}

static int proc_get_cam(struct seq_file *m, void *v)
{
	struct net_device *dev = m->private;
	_adapter *adapter = (_adapter *)rtw_netdev_priv(dev);
	u8 i;

	return 0;
}

static ssize_t proc_set_cam(struct file *file, const char __user *buffer, size_t count, loff_t *pos, void *data)
{
	struct net_device *dev = data;
	_adapter *adapter;

	char tmp[32];
	char cmd[4];
	u8 id;

	adapter = (_adapter *)rtw_netdev_priv(dev);
	if (!adapter)
		return -EFAULT;

	if (buffer && !copy_from_user(tmp, buffer, sizeof(tmp))) {

		/* c <id>: clear specific cam entry */
		/* wfc <id>: write specific cam entry from cam cache */

		int num = sscanf(tmp, "%s %hhu", cmd, &id);

		if (num < 2)
			return count;

		if (strcmp("c", cmd) == 0) {
			_clear_cam_entry(adapter, id);
			adapter->securitypriv.hw_decrypted = _FALSE; /* temporarily set this for TX path to use SW enc */
		} else if (strcmp("wfc", cmd) == 0) {
			write_cam_from_cache(adapter, id);
		}
	}

	return count;
}

static int proc_get_cam_cache(struct seq_file *m, void *v)
{
	struct net_device *dev = m->private;
	_adapter *adapter = (_adapter *)rtw_netdev_priv(dev);
	struct dvobj_priv *dvobj = adapter_to_dvobj(adapter);
	u8 i;

	DBG_871X_SEL_NL(m, "cam bitmap:0x%016llx\n", dvobj->cam_ctl.bitmap);

	DBG_871X_SEL_NL(m, "%-2s %-6s %-17s %-32s %-3s %-7s"
		//" %-2s %-2s %-4s %-5s"
		"\n"
		, "id", "ctrl", "addr", "key", "kid", "type"
		//, "MK", "GK", "MFB", "valid"
	);

	for (i=0;i<32;i++) {
		if (dvobj->cam_cache[i].ctrl != 0)
			DBG_871X_SEL_NL(m, "%2u 0x%04x "MAC_FMT" "KEY_FMT" %3u %-7s"
				//" %2u %2u 0x%02x %5u"
				"\n", i
				, dvobj->cam_cache[i].ctrl
				, MAC_ARG(dvobj->cam_cache[i].mac)
				, KEY_ARG(dvobj->cam_cache[i].key)
				, (dvobj->cam_cache[i].ctrl)&0x03
				, security_type_str(((dvobj->cam_cache[i].ctrl)>>2)&0x07)
				//, ((dvobj->cam_cache[i].ctrl)>>5)&0x01
				//, ((dvobj->cam_cache[i].ctrl)>>6)&0x01
				//, ((dvobj->cam_cache[i].ctrl)>>8)&0x7f
				//, ((dvobj->cam_cache[i].ctrl)>>15)&0x01
			);
	}

	return 0;
}

/*
* rtw_adapter_proc:
* init/deinit when register/unregister net_device
*/
const struct rtw_proc_hdl adapter_proc_hdls [] = {
	{"write_reg", proc_get_dummy, proc_set_write_reg},
	{"read_reg", proc_get_read_reg, proc_set_read_reg},
	{"fwstate", proc_get_fwstate, NULL},
	{"sec_info", proc_get_sec_info, NULL},
	{"mlmext_state", proc_get_mlmext_state, NULL},
	{"qos_option", proc_get_qos_option, NULL},
	{"ht_option", proc_get_ht_option, NULL},
	{"rf_info", proc_get_rf_info, NULL},
	{"survey_info", proc_get_survey_info, NULL},
	{"ap_info", proc_get_ap_info, NULL},
	{"adapter_state", proc_get_adapter_state, NULL},
	{"trx_info", proc_get_trx_info, NULL},
	{"rate_ctl", proc_get_rate_ctl, proc_set_rate_ctl},
	{"mac_qinfo", proc_get_mac_qinfo, NULL},
	{"cam", proc_get_cam, proc_set_cam},
	{"cam_cache", proc_get_cam_cache, NULL},
	{"hw_info", proc_get_hw_status, NULL},
	{"rx_info", proc_get_rx_info, proc_reset_rx_info},
	{"wifi_spec",proc_get_wifi_spec, NULL},

#ifdef CONFIG_LAYER2_ROAMING
	{"roam_flags", proc_get_roam_flags, proc_set_roam_flags},
	{"roam_param", proc_get_roam_param, proc_set_roam_param},
	{"roam_tgt_addr", proc_get_dummy, proc_set_roam_tgt_addr},
#endif /* CONFIG_LAYER2_ROAMING */

	{"wait_hiq_empty", proc_get_dummy, proc_set_wait_hiq_empty},

	{"mac_reg_dump", proc_get_mac_reg_dump, NULL},
	{"bb_reg_dump", proc_get_bb_reg_dump, NULL},
	{"rf_reg_dump", proc_get_rf_reg_dump, NULL},

#ifdef CONFIG_AP_MODE
	{"all_sta_info", proc_get_all_sta_info, NULL},
#endif

#ifdef CONFIG_FIND_BEST_CHANNEL

	{"best_channel", proc_get_best_channel, proc_set_best_channel},
#endif

	{"rx_signal", proc_get_rx_signal, proc_set_rx_signal},

	{"ht_enable", proc_get_ht_enable, proc_set_ht_enable},
	{"cbw40_enable", proc_get_cbw40_enable, proc_set_cbw40_enable},
	{"ampdu_enable", proc_get_ampdu_enable, proc_set_ampdu_enable},
	{"rx_stbc", proc_get_rx_stbc, proc_set_rx_stbc},

	{"path_rssi", proc_get_two_path_rssi, NULL},
	{"rssi_disp", proc_get_rssi_disp, proc_set_rssi_disp},

	{"vid", proc_get_vid, NULL},
	{"pid", proc_get_pid, NULL},

#if defined(DBG_CONFIG_ERROR_DETECT)
	{"sreset", proc_get_sreset, proc_set_sreset},
#endif /* DBG_CONFIG_ERROR_DETECT */
};

const int adapter_proc_hdls_num = sizeof(adapter_proc_hdls) / sizeof(struct rtw_proc_hdl);

static int rtw_adapter_proc_open(struct inode *inode, struct file *file)
{
#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 17, 0)
	ssize_t index = (ssize_t)PDE_DATA(inode);
#else
	ssize_t index = (ssize_t)pde_data(inode);
#endif
	const struct rtw_proc_hdl *hdl = adapter_proc_hdls+index;

	return single_open(file, hdl->show, proc_get_parent_data(inode));
}

static ssize_t rtw_adapter_proc_write(struct file *file, const char __user *buffer, size_t count, loff_t *pos)
{
#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 17, 0)
	ssize_t index = (ssize_t)PDE_DATA(file_inode(file));
#else
	ssize_t index = (ssize_t)pde_data(file_inode(file));
#endif
	const struct rtw_proc_hdl *hdl = adapter_proc_hdls+index;
	ssize_t (*write)(struct file *, const char __user *, size_t, loff_t *, void *) = hdl->write;

	if (write)
		return write(file, buffer, count, pos, ((struct seq_file *)file->private_data)->private);

	return -EROFS;
}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5,6,0))
static const struct proc_ops rtw_adapter_proc_ops = {
	.proc_open = rtw_adapter_proc_open,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_release = single_release,
	.proc_write = rtw_adapter_proc_write,
};
#else
static const struct file_operations rtw_adapter_proc_fops = {
	.owner = THIS_MODULE,
	.open = rtw_adapter_proc_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
	.write = rtw_adapter_proc_write,
};
#endif

int proc_get_dm_ability(struct seq_file *m, void *v)
{
	struct net_device *dev = m->private;
	_adapter *adapter = (_adapter *)rtw_netdev_priv(dev);

	rtw_dm_ability_msg(m, adapter);

	return 0;
}

ssize_t proc_set_dm_ability(struct file *file, const char __user *buffer, size_t count, loff_t *pos, void *data)
{
	struct net_device *dev = data;
	_adapter *adapter = (_adapter *)rtw_netdev_priv(dev);
	char tmp[32];

	u8 ability;

	if (count < 1)
		return -EFAULT;

	if (buffer && !copy_from_user(tmp, buffer, sizeof(tmp))) {

		int num = sscanf(tmp, "%hhx", &ability);

		if (num != 1)
			return count;

		rtw_dm_ability_set(adapter, ability);
	}

	return count;
}

int proc_get_odm_dbg_comp(struct seq_file *m, void *v)
{
	struct net_device *dev = m->private;
	_adapter *adapter = (_adapter *)rtw_netdev_priv(dev);

	rtw_odm_dbg_comp_msg(m, adapter);

	return 0;
}

ssize_t proc_set_odm_dbg_comp(struct file *file, const char __user *buffer, size_t count, loff_t *pos, void *data)
{
	struct net_device *dev = data;
	_adapter *adapter = (_adapter *)rtw_netdev_priv(dev);
	char tmp[32];

	u64 dbg_comp;

	if (count < 1)
		return -EFAULT;

	if (buffer && !copy_from_user(tmp, buffer, sizeof(tmp))) {

		int num = sscanf(tmp, "%llx", &dbg_comp);

		if (num != 1)
			return count;

		rtw_odm_dbg_comp_set(adapter, dbg_comp);
	}

	return count;
}

int proc_get_odm_dbg_level(struct seq_file *m, void *v)
{
	struct net_device *dev = m->private;
	_adapter *adapter = (_adapter *)rtw_netdev_priv(dev);

	rtw_odm_dbg_level_msg(m, adapter);

	return 0;
}

ssize_t proc_set_odm_dbg_level(struct file *file, const char __user *buffer, size_t count, loff_t *pos, void *data)
{
	struct net_device *dev = data;
	_adapter *adapter = (_adapter *)rtw_netdev_priv(dev);
	char tmp[32];

	u32 dbg_level;

	if (count < 1)
		return -EFAULT;

	if (buffer && !copy_from_user(tmp, buffer, sizeof(tmp))) {

		int num = sscanf(tmp, "%u", &dbg_level);

		if (num != 1)
			return count;

		rtw_odm_dbg_level_set(adapter, dbg_level);
	}

	return count;
}

#ifdef CONFIG_ODM_ADAPTIVITY
int proc_get_odm_adaptivity(struct seq_file *m, void *v)
{
	struct net_device *dev = m->private;
	_adapter *padapter = (_adapter *)rtw_netdev_priv(dev);

	rtw_odm_adaptivity_parm_msg(m, padapter);

	return 0;
}

ssize_t proc_set_odm_adaptivity(struct file *file, const char __user *buffer, size_t count, loff_t *pos, void *data)
{
	struct net_device *dev = data;
	_adapter *padapter = (_adapter *)rtw_netdev_priv(dev);
	char tmp[32];
	u32 TH_L2H_ini;
	s8 TH_EDCCA_HL_diff;
	u32 IGI_Base;
	int ForceEDCCA;
	u8 AdapEn_RSSI;
	u8 IGI_LowerBound;

	if (count < 1)
		return -EFAULT;

	if (buffer && !copy_from_user(tmp, buffer, sizeof(tmp))) {

		int num = sscanf(tmp, "%x %hhd %x %d %hhu %hhu",
			&TH_L2H_ini, &TH_EDCCA_HL_diff, &IGI_Base, &ForceEDCCA, &AdapEn_RSSI, &IGI_LowerBound);

		if (num != 6)
			return count;

		rtw_odm_adaptivity_parm_set(padapter, (s8)TH_L2H_ini, TH_EDCCA_HL_diff, (s8)IGI_Base, (bool)ForceEDCCA, AdapEn_RSSI, IGI_LowerBound);
	}

	return count;
}
#endif /* CONFIG_ODM_ADAPTIVITY */

/*
* rtw_dm_proc:
* init/deinit when register/unregister net_device, along with rtw_adapter_proc
*/
const struct rtw_proc_hdl dm_proc_hdls [] = {
	{"dbg_comp", proc_get_odm_dbg_comp, proc_set_odm_dbg_comp},
	{"dbg_level", proc_get_odm_dbg_level, proc_set_odm_dbg_level},
	{"ability", proc_get_dm_ability, proc_set_dm_ability},
#ifdef CONFIG_ODM_ADAPTIVITY
	{"adaptivity", proc_get_odm_adaptivity, proc_set_odm_adaptivity},
#endif
};

const int dm_proc_hdls_num = sizeof(dm_proc_hdls) / sizeof(struct rtw_proc_hdl);

static int rtw_dm_proc_open(struct inode *inode, struct file *file)
{
#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 17, 0)
	ssize_t index = (ssize_t)PDE_DATA(inode);
#else
	ssize_t index = (ssize_t)pde_data(inode);
#endif
	const struct rtw_proc_hdl *hdl = dm_proc_hdls+index;

	return single_open(file, hdl->show, proc_get_parent_data(inode));
}

static ssize_t rtw_dm_proc_write(struct file *file, const char __user *buffer, size_t count, loff_t *pos)
{
#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 17, 0)
	ssize_t index = (ssize_t)PDE_DATA(file_inode(file));
#else
	ssize_t index = (ssize_t)pde_data(file_inode(file));
#endif
	const struct rtw_proc_hdl *hdl = dm_proc_hdls+index;
	ssize_t (*write)(struct file *, const char __user *, size_t, loff_t *, void *) = hdl->write;

	if (write)
		return write(file, buffer, count, pos, ((struct seq_file *)file->private_data)->private);

	return -EROFS;
}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5,6,0))
static const struct proc_ops rtw_dm_proc_ops = {
	.proc_open = rtw_dm_proc_open,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_release = single_release,
	.proc_write = rtw_dm_proc_write,
};
#else
static const struct file_operations rtw_dm_proc_fops = {
	.owner = THIS_MODULE,
	.open = rtw_dm_proc_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
	.write = rtw_dm_proc_write,
};
#endif

struct proc_dir_entry *rtw_dm_proc_init(struct net_device *dev)
{
	struct proc_dir_entry *dir_dm = NULL;
	struct proc_dir_entry *entry = NULL;
	_adapter	*adapter = rtw_netdev_priv(dev);
	ssize_t i;

	if (adapter->dir_dev == NULL) {
		rtw_warn_on(1);
		goto exit;
	}

	if (adapter->dir_dm != NULL) {
		rtw_warn_on(1);
		goto exit;
	}

	dir_dm = rtw_proc_create_dir("dm", adapter->dir_dev, dev);
	if (dir_dm == NULL) {
		rtw_warn_on(1);
		goto exit;
	}

	adapter->dir_dm = dir_dm;

	for (i=0;i<dm_proc_hdls_num;i++) {
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5,6,0))
		entry = rtw_proc_create_entry(dm_proc_hdls[i].name, dir_dm, &rtw_dm_proc_ops, (void *)i);
#else
		entry = rtw_proc_create_entry(dm_proc_hdls[i].name, dir_dm, &rtw_dm_proc_fops, (void *)i);
#endif
		if (!entry) {
			rtw_warn_on(1);
			goto exit;
		}
	}

exit:
	return dir_dm;
}

void rtw_dm_proc_deinit(_adapter	*adapter)
{
	struct proc_dir_entry *dir_dm = NULL;
	int i;

	dir_dm = adapter->dir_dm;

	if (dir_dm == NULL) {
		rtw_warn_on(1);
		return;
	}

	for (i=0;i<dm_proc_hdls_num;i++)
		remove_proc_entry(dm_proc_hdls[i].name, dir_dm);

	remove_proc_entry("dm", adapter->dir_dev);

	adapter->dir_dm = NULL;
}

struct proc_dir_entry *rtw_adapter_proc_init(struct net_device *dev)
{
	struct proc_dir_entry *drv_proc = get_rtw_drv_proc();
	struct proc_dir_entry *dir_dev = NULL;
	struct proc_dir_entry *entry = NULL;
	_adapter *adapter = rtw_netdev_priv(dev);
	u8 rf_type;
	ssize_t i;

	if (drv_proc == NULL) {
		rtw_warn_on(1);
		goto exit;
	}

	if (adapter->dir_dev != NULL) {
		rtw_warn_on(1);
		goto exit;
	}

	dir_dev = rtw_proc_create_dir(dev->name, drv_proc, dev);
	if (dir_dev == NULL) {
		rtw_warn_on(1);
		goto exit;
	}

	adapter->dir_dev = dir_dev;

	for (i=0;i<adapter_proc_hdls_num;i++) {
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5,6,0))
		entry = rtw_proc_create_entry(adapter_proc_hdls[i].name, dir_dev, &rtw_adapter_proc_ops, (void *)i);
#else
		entry = rtw_proc_create_entry(adapter_proc_hdls[i].name, dir_dev, &rtw_adapter_proc_fops, (void *)i);
#endif
		if (!entry) {
			rtw_warn_on(1);
			goto exit;
		}
	}

	rtw_dm_proc_init(dev);

exit:
	return dir_dev;
}

void rtw_adapter_proc_deinit(struct net_device *dev)
{
	struct proc_dir_entry *drv_proc = get_rtw_drv_proc();
	struct proc_dir_entry *dir_dev = NULL;
	_adapter *adapter = rtw_netdev_priv(dev);
	int i;

	dir_dev = adapter->dir_dev;

	if (dir_dev == NULL) {
		rtw_warn_on(1);
		return;
	}

	for (i=0;i<adapter_proc_hdls_num;i++)
		remove_proc_entry(adapter_proc_hdls[i].name, dir_dev);

	rtw_dm_proc_deinit(adapter);

	remove_proc_entry(dev->name, drv_proc);

	adapter->dir_dev = NULL;
}

void rtw_adapter_proc_replace(struct net_device *dev)
{
	struct proc_dir_entry *drv_proc = get_rtw_drv_proc();
	struct proc_dir_entry *dir_dev = NULL;
	_adapter *adapter = rtw_netdev_priv(dev);
	int i;

	dir_dev = adapter->dir_dev;

	if (dir_dev == NULL) {
		rtw_warn_on(1);
		return;
	}

	for (i=0;i<adapter_proc_hdls_num;i++)
		remove_proc_entry(adapter_proc_hdls[i].name, dir_dev);

	rtw_dm_proc_deinit(adapter);

	remove_proc_entry(adapter->old_ifname, drv_proc);

	adapter->dir_dev = NULL;

	rtw_adapter_proc_init(dev);

}

#endif /* CONFIG_PROC_DEBUG */
