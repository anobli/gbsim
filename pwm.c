/*
 * Greybus Simulator
 *
 * Copyright 2014 Google Inc.
 * Copyright 2014 Linaro Ltd.
 *
 * Provided under the three clause BSD license found in the LICENSE file.
 */

#include <fcntl.h>
#include <libsoc_pwm.h>
#include <linux/fs.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>

#include "gbsim.h"

static int pwm_on[2];
static pwm *pwms[2];

void pwm_handler(uint16_t cport_id, void *rbuf, size_t rsize,
					void *tbuf, size_t tsize)
{
	struct op_header *oph;
	struct op_msg *op_req = rbuf;
	struct op_msg *op_rsp;
	size_t sz;
	__u32 duty;
	__u32 period;
	uint8_t module_id;

	module_id = cport_to_module_id(cport_id);

	op_rsp = (struct op_msg *)tbuf;
	oph = (struct op_header *)&op_req->header;

	/* Store the cport id in the header pad bytes */
	op_rsp->header.pad[0] = cport_id & 0xff;
	op_rsp->header.pad[1] = (cport_id >> 8) & 0xff;

	switch (oph->type) {
	case GB_PWM_TYPE_PROTOCOL_VERSION:
		sz = sizeof(struct op_header) +
				      sizeof(struct protocol_version_rsp);
		op_rsp->header.size = htole16((__u16)sz);
		op_rsp->header.id = oph->id;
		op_rsp->header.type = OP_RESPONSE | GB_PWM_TYPE_PROTOCOL_VERSION;
		op_rsp->header.result = PROTOCOL_STATUS_SUCCESS;
		op_rsp->pv_rsp.version_major = GREYBUS_VERSION_MAJOR;
		op_rsp->pv_rsp.version_minor = GREYBUS_VERSION_MINOR;
		gbsim_debug("Module %hhu -> AP CPort %hu PWM protocol version response\n  ",
			    module_id, cport_id);
		if (verbose)
			gbsim_dump((__u8 *)op_rsp, sz);
		write(to_ap, op_rsp, sz);
		break;
	case GB_PWM_TYPE_PWM_COUNT:
		sz = sizeof(struct op_header) + sizeof(struct gb_pwm_count_response);
		op_rsp->header.size = htole16((__u16)sz);
		op_rsp->header.id = oph->id;
		op_rsp->header.type = OP_RESPONSE | GB_PWM_TYPE_PWM_COUNT;
		op_rsp->header.result = PROTOCOL_STATUS_SUCCESS;
		op_rsp->pwm_cnt_rsp.count = 1; /* Something arbitrary, but useful */
		gbsim_debug("Module %hhu -> AP CPort %hu PWM count response\n  ",
			    module_id, cport_id);
		if (verbose)
			gbsim_dump((__u8 *)op_rsp, sz);
		write(to_ap, op_rsp, sz);
		break;
	case GB_PWM_TYPE_ACTIVATE:
		sz = sizeof(struct op_header) + 0;
		op_rsp->header.size = htole16((__u16)sz);
		op_rsp->header.id = oph->id;
		op_rsp->header.type = OP_RESPONSE | GB_PWM_TYPE_ACTIVATE;
		op_rsp->header.result = PROTOCOL_STATUS_SUCCESS;
		gbsim_debug("AP -> Module %hhu CPort %hu PWM %d activate request\n  ",
			    module_id, cport_id, op_req->pwm_act_req.which);
		if (verbose)
			gbsim_dump((__u8 *)op_rsp, sz);
		write(to_ap, op_rsp, sz);
		break;
	case GB_PWM_TYPE_DEACTIVATE:
		sz = sizeof(struct op_header) + 0;
		op_rsp->header.size = htole16((__u16)sz);
		op_rsp->header.id = oph->id;
		op_rsp->header.type = OP_RESPONSE | GB_PWM_TYPE_DEACTIVATE;
		op_rsp->header.result = PROTOCOL_STATUS_SUCCESS;
		gbsim_debug("AP -> Module %hhu CPort %hu PWM %d deactivate request\n  ",
			    module_id, cport_id, op_req->pwm_deact_req.which);
		if (verbose)
			gbsim_dump((__u8 *)op_rsp, sz);
		write(to_ap, op_rsp, sz);
		break;
	case GB_PWM_TYPE_CONFIG:
		sz = sizeof(struct op_header) + 0;
		duty = le32toh(op_req->pwm_cfg_req.duty);
		period = le32toh(op_req->pwm_cfg_req.period);
		op_rsp->header.size = htole16((__u16)sz);
		op_rsp->header.id = oph->id;
		op_rsp->header.type = OP_RESPONSE | GB_PWM_TYPE_CONFIG;
		op_rsp->header.result = PROTOCOL_STATUS_SUCCESS;
		if (bbb_backend) {
			libsoc_pwm_set_duty_cycle(pwms[op_req->pwm_cfg_req.which], duty);
			libsoc_pwm_set_period(pwms[op_req->pwm_cfg_req.which], period);
		}
		gbsim_debug("AP -> Module %hhu CPort %hu PWM %d config (%dns/%dns) request\n  ",
			    module_id, cport_id, op_req->pwm_cfg_req.which, duty, period);
		if (verbose)
			gbsim_dump((__u8 *)op_rsp, sz);
		write(to_ap, op_rsp, sz);
		break;
	case GB_PWM_TYPE_POLARITY:
		sz = sizeof(struct op_header) + 0;
		op_rsp->header.size = htole16((__u16)sz);
		op_rsp->header.id = oph->id;
		op_rsp->header.type = OP_RESPONSE | GB_PWM_TYPE_POLARITY;
		if (pwm_on[op_req->pwm_pol_req.which])
			op_rsp->header.result = PROTOCOL_STATUS_BUSY;
		else {
			op_rsp->header.result = PROTOCOL_STATUS_SUCCESS;
			if (bbb_backend)
				libsoc_pwm_set_polarity(pwms[op_req->pwm_pol_req.which],
						    op_req->pwm_pol_req.polarity);
		}
		gbsim_debug("AP -> Module %hhu CPort %hu PWM %d polarity (%s) request\n  ",
			    module_id,
			    cport_id, op_req->pwm_cfg_req.which,
			    op_req->pwm_pol_req.polarity ? "inverse" : "normal");
		if (verbose)
			gbsim_dump((__u8 *)op_rsp, sz);
		write(to_ap, op_rsp, sz);
		break;
	case GB_PWM_TYPE_ENABLE:
		sz = sizeof(struct op_header) + 0;
		op_rsp->header.size = htole16((__u16)sz);
		op_rsp->header.id = oph->id;
		op_rsp->header.type = OP_RESPONSE | GB_PWM_TYPE_ENABLE;
		op_rsp->header.result = PROTOCOL_STATUS_SUCCESS;
		pwm_on[op_req->pwm_enb_req.which] = 1;
		if (bbb_backend)
			libsoc_pwm_set_enabled(pwms[op_req->pwm_enb_req.which], ENABLED);
		gbsim_debug("AP -> Module %hhu CPort %hu PWM %d enable request\n  ",
			    module_id, cport_id, op_req->pwm_enb_req.which);
		if (verbose)
			gbsim_dump((__u8 *)op_rsp, sz);
		write(to_ap, op_rsp, sz);
		break;
	case GB_PWM_TYPE_DISABLE:
		sz = sizeof(struct op_header) + 0;
		op_rsp->header.size = htole16((__u16)sz);
		op_rsp->header.id = oph->id;
		op_rsp->header.type = OP_RESPONSE | GB_PWM_TYPE_DISABLE;
		op_rsp->header.result = PROTOCOL_STATUS_SUCCESS;
		pwm_on[op_req->pwm_dis_req.which] = 0;
		if (bbb_backend)
			libsoc_pwm_set_enabled(pwms[op_req->pwm_dis_req.which], DISABLED);
		gbsim_debug("AP -> Module %hhu CPort %hu PWM %d disable request\n  ",
			    module_id, cport_id, op_req->pwm_dis_req.which);
		if (verbose)
			gbsim_dump((__u8 *)op_rsp, sz);
		write(to_ap, op_rsp, sz);
		break;
	default:
		gbsim_error("pwm operation type %02x not supported\n", oph->type);
	}
}

void pwm_init(void)
{
	if (bbb_backend) {
		/* Grab PWM0A and PWM0B found on P9-31 and P9-29 */
		pwms[0] = libsoc_pwm_request(0, 0, LS_GREEDY);
		pwms[1] = libsoc_pwm_request(0, 1, LS_GREEDY);
	}
}
