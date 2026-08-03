package com.nakamura_labs.ladenzeit.repository;

import java.time.Instant;

import org.springframework.data.jpa.repository.JpaRepository;
import org.springframework.stereotype.Repository;

import com.nakamura_labs.ladenzeit.entity.LZPlace;

import jakarta.transaction.Transactional;

@Repository
public interface LZPlaceRepository extends JpaRepository<LZPlace, String> {

	@Transactional
	void deleteByCreatedAtBefore(Instant expiryDate);
}
