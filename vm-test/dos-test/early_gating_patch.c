/**
 * 早期门控机制 - 模拟签名证书检查
 *
 * 功能：在 IKE_INTERMEDIATE #0 阶段（双证书分发），检查签名证书有效性
 * 如果证书无效，提前终止协商，避免后续昂贵的 SM2-KEM/ML-KEM 运算
 *
 * 位置：ike_init.c 的 process_r_multi_ke 函数
 */

/* ==================== 门控检查函数 ==================== */

/**
 * 模拟签名证书验证检查
 * @param message IKE 消息
 * @return TRUE = 证书有效，继续流程
 *         FALSE = 证书无效，终止协商
 */
static bool early_gating_check_cert(message_t *message)
{
    enumerator_t *cert_enum;
    payload_t *cert_payload;
    bool has_valid_cert = FALSE;
    int cert_count = 0;

    /* 检查环境变量控制门控行为（用于测试） */
    const char *gating_mode = getenv("DOS_GATING_MODE");

    /* 如果设置了 DOS_GATING_MODE=block，模拟阻止所有请求 */
    if (gating_mode && strcmp(gating_mode, "block") == 0)
    {
        DBG1(DBG_IKE, "Early Gating: BLOCK mode - rejecting all requests (test)");
        return FALSE;
    }

    /* 遍历消息中的证书载荷 */
    cert_enum = message->create_payload_enumerator(message);
    while (cert_enum->enumerate(cert_enum, &cert_payload))
    {
        if (cert_payload->get_type(cert_payload) == PLV2_CERTIFICATE)
        {
            cert_payload_t *cp = (cert_payload_t*)cert_payload;
            chunk_t cert_data = cp->get_data(cp);
            cert_count++;

            /* 简化的证书检查：
             * 1. 检查证书是否存在
             * 2. 检查证书长度是否合理
             * 3. 可选：检查证书中的特定标记（如 "INVALID" 字符串）
             */

            if (cert_data.len > 100)
            {
                /* 检查证书 DER 编码中是否包含 "INVALID" 标记 */
                bool has_invalid_marker = FALSE;
                for (size_t i = 0; i < cert_data.len - 7; i++)
                {
                    if (cert_data.ptr[i]   == 'I' &&
                        cert_data.ptr[i+1] == 'N' &&
                        cert_data.ptr[i+2] == 'V' &&
                        cert_data.ptr[i+3] == 'A' &&
                        cert_data.ptr[i+4] == 'L' &&
                        cert_data.ptr[i+5] == 'I' &&
                        cert_data.ptr[i+6] == 'D')
                    {
                        has_invalid_marker = TRUE;
                        break;
                    }
                }

                if (!has_invalid_marker)
                {
                    /* 证书未发现攻击标记，视为有效 */
                    has_valid_cert = TRUE;
                }
                else
                {
                    DBG1(DBG_IKE, "Early Gating ALERT: Found 'INVALID' marker in certificate!");
                }
            }
        }
    }
    cert_enum->destroy(cert_enum);

    /* 如果没有收到任何证书，拒绝请求 */
    if (cert_count == 0)
    {
        DBG1(DBG_IKE, "Early Gating: No certificate received in IKE_INTERMEDIATE #0");
        return FALSE;
    }

    return has_valid_cert;
}

/* ==================== 修改 process_r_multi_ke 函数 ==================== */

/*
原始代码（ike_init.c 行 1116-1135）：

METHOD(task_t, process_r_multi_ke, status_t,
    private_ike_init_t *this, message_t *message)
{
    if (message->get_exchange_type(message) == exchange_type_multi_ke(this))
    {
        ke_payload_t *ke = (ke_payload_t*)message->get_payload(message, PLV2_KEY_EXCHANGE);
        if (ke)
        {
            process_payloads_multi_ke(this, message);
        }
        else
        {
            DBG1(DBG_IKE, "PQ-GM-IKEv2: IKE_INTERMEDIATE #%d - no KE payload, certificates only",
                 this->intermediate_round);
        }
    }
    return NEED_MORE;
}

修改后的代码：
*/

METHOD(task_t, process_r_multi_ke, status_t,
    private_ike_init_t *this, message_t *message)
{
    if (message->get_exchange_type(message) == exchange_type_multi_ke(this))
    {
        ke_payload_t *ke = (ke_payload_t*)message->get_payload(message, PLV2_KEY_EXCHANGE);
        if (ke)
        {
            process_payloads_multi_ke(this, message);
        }
        else
        {
            /* ===== 早期门控检查 ===== */
            if (!early_gating_check_cert(message))
            {
                DBG1(DBG_IKE, "Early Gating: Invalid certificate detected, aborting IKE_SA!");
                return FAILED;  /* 门控生效：提前终止，不消耗后续资源 */
            }
            DBG1(DBG_IKE, "Early Gating: Certificate verified, continuing...");
            /* ===== 门控检查结束 ===== */

            DBG1(DBG_IKE, "PQ-GM-IKEv2: IKE_INTERMEDIATE #%d - no KE payload, certificates only",
                 this->intermediate_round);
            /* Don't increment intermediate_round here - build_r_multi_ke will do it */
        }
    }
    return NEED_MORE;
}

/* ==================== 测试说明 ==================== */

/*
测试方法：

1. 无门控测试（基线）：
   - 使用原始代码（不修改）
   - 或设置环境变量：export DOS_GATING_MODE=allow

2. 有门控测试：
   - 应用上述修改
   - 设置环境变量：export DOS_GATING_MODE=block（阻止所有请求）
   - 或使用带 "INVALID" 标记的证书

3. 编译步骤：
   cd /home/ipsec/strongswan
   make -j$(nproc)
   sudo make install
   sudo systemctl restart charon  # 或重启 VM

4. 环境变量设置：
   在启动 charon 前设置：
   export DOS_GATING_MODE=block

   或在 systemd 服务中添加：
   Environment="DOS_GATING_MODE=block"
*/