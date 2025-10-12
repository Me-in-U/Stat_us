package com.meinu.status.api.ingest.repository;

import com.meinu.status.api.ingest.entity.VscodeEvent;
import org.springframework.data.jpa.repository.JpaRepository;

public interface VscodeEventRepository extends JpaRepository<VscodeEvent, Long> {
}
